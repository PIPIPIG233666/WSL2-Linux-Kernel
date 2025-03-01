// SPDX-License-Identifier: GPL-2.0
//
// kselftest for the ALSA mixer API
//
// Original author: Mark Brown <broonie@kernel.org>
// Copyright (c) 2021 Arm Limited

// This test will iterate over all cards detected in the system, exercising
// every mixer control it can find.  This may conflict with other system
// software if there is audio activity so is best run on a system with a
// minimal active userspace.

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <getopt.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <alsa/asoundlib.h>
#include <poll.h>
#include <stdint.h>

#include "../kselftest.h"

#define TESTS_PER_CONTROL 4

struct card_data {
	snd_ctl_t *handle;
	int card;
	int num_ctls;
	snd_ctl_elem_list_t *ctls;
	struct card_data *next;
};

struct ctl_data {
	const char *name;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_value_t *def_val;
	int elem;
	struct card_data *card;
	struct ctl_data *next;
};

static const char *alsa_config =
"ctl.hw {\n"
"	@args [ CARD ]\n"
"	@args.CARD.type string\n"
"	type hw\n"
"	card $CARD\n"
"}\n"
;

int num_cards = 0;
int num_controls = 0;
struct card_data *card_list = NULL;
struct ctl_data *ctl_list = NULL;

#ifdef SND_LIB_VER
#if SND_LIB_VERSION >= SND_LIB_VER(1, 2, 6)
#define LIB_HAS_LOAD_STRING
#endif
#endif

#ifndef LIB_HAS_LOAD_STRING
int snd_config_load_string(snd_config_t **config, const char *s, size_t size)
{
	snd_input_t *input;
	snd_config_t *dst;
	int err;

	assert(config && s);
	if (size == 0)
		size = strlen(s);
	err = snd_input_buffer_open(&input, s, size);
	if (err < 0)
		return err;
	err = snd_config_top(&dst);
	if (err < 0) {
		snd_input_close(input);
		return err;
	}
	err = snd_config_load(dst, input);
	snd_input_close(input);
	if (err < 0) {
		snd_config_delete(dst);
		return err;
	}
	*config = dst;
	return 0;
}
#endif

void find_controls(void)
{
	char name[32];
	int card, ctl, err;
	struct card_data *card_data;
	struct ctl_data *ctl_data;
	snd_config_t *config;

	card = -1;
	if (snd_card_next(&card) < 0 || card < 0)
		return;

	err = snd_config_load_string(&config, alsa_config, strlen(alsa_config));
	if (err < 0) {
		ksft_print_msg("Unable to parse custom alsa-lib configuration: %s\n",
			       snd_strerror(err));
		ksft_exit_fail();
	}

	while (card >= 0) {
		sprintf(name, "hw:%d", card);

		card_data = malloc(sizeof(*card_data));
		if (!card_data)
			ksft_exit_fail_msg("Out of memory\n");

		err = snd_ctl_open_lconf(&card_data->handle, name, 0, config);
		if (err < 0) {
			ksft_print_msg("Failed to get hctl for card %d: %s\n",
				       card, snd_strerror(err));
			goto next_card;
		}

		/* Count controls */
		snd_ctl_elem_list_malloc(&card_data->ctls);
		snd_ctl_elem_list(card_data->handle, card_data->ctls);
		card_data->num_ctls = snd_ctl_elem_list_get_count(card_data->ctls);

		/* Enumerate control information */
		snd_ctl_elem_list_alloc_space(card_data->ctls, card_data->num_ctls);
		snd_ctl_elem_list(card_data->handle, card_data->ctls);

		card_data->card = num_cards++;
		card_data->next = card_list;
		card_list = card_data;

		num_controls += card_data->num_ctls;

		for (ctl = 0; ctl < card_data->num_ctls; ctl++) {
			ctl_data = malloc(sizeof(*ctl_data));
			if (!ctl_data)
				ksft_exit_fail_msg("Out of memory\n");

			ctl_data->card = card_data;
			ctl_data->elem = ctl;
			ctl_data->name = snd_ctl_elem_list_get_name(card_data->ctls,
								    ctl);

			err = snd_ctl_elem_id_malloc(&ctl_data->id);
			if (err < 0)
				ksft_exit_fail_msg("Out of memory\n");

			err = snd_ctl_elem_info_malloc(&ctl_data->info);
			if (err < 0)
				ksft_exit_fail_msg("Out of memory\n");

			err = snd_ctl_elem_value_malloc(&ctl_data->def_val);
			if (err < 0)
				ksft_exit_fail_msg("Out of memory\n");

			snd_ctl_elem_list_get_id(card_data->ctls, ctl,
						 ctl_data->id);
			snd_ctl_elem_info_set_id(ctl_data->info, ctl_data->id);
			err = snd_ctl_elem_info(card_data->handle,
						ctl_data->info);
			if (err < 0) {
				ksft_print_msg("%s getting info for %d\n",
					       snd_strerror(err),
					       ctl_data->name);
			}

			snd_ctl_elem_value_set_id(ctl_data->def_val,
						  ctl_data->id);

			ctl_data->next = ctl_list;
			ctl_list = ctl_data;
		}

	next_card:
		if (snd_card_next(&card) < 0) {
			ksft_print_msg("snd_card_next");
			break;
		}
	}

	snd_config_delete(config);
}

bool ctl_value_index_valid(struct ctl_data *ctl, snd_ctl_elem_value_t *val,
			   int index)
{
	long int_val;
	long long int64_val;

	switch (snd_ctl_elem_info_get_type(ctl->info)) {
	case SND_CTL_ELEM_TYPE_NONE:
		ksft_print_msg("%s.%d Invalid control type NONE\n",
			       ctl->name, index);
		return false;

	case SND_CTL_ELEM_TYPE_BOOLEAN:
		int_val = snd_ctl_elem_value_get_boolean(val, index);
		switch (int_val) {
		case 0:
		case 1:
			break;
		default:
			ksft_print_msg("%s.%d Invalid boolean value %ld\n",
				       ctl->name, index, int_val);
			return false;
		}
		break;

	case SND_CTL_ELEM_TYPE_INTEGER:
		int_val = snd_ctl_elem_value_get_integer(val, index);

		if (int_val < snd_ctl_elem_info_get_min(ctl->info)) {
			ksft_print_msg("%s.%d value %ld less than minimum %ld\n",
				       ctl->name, index, int_val,
				       snd_ctl_elem_info_get_min(ctl->info));
			return false;
		}

		if (int_val > snd_ctl_elem_info_get_max(ctl->info)) {
			ksft_print_msg("%s.%d value %ld more than maximum %ld\n",
				       ctl->name, index, int_val,
				       snd_ctl_elem_info_get_max(ctl->info));
			return false;
		}

		/* Only check step size if there is one and we're in bounds */
		if (snd_ctl_elem_info_get_step(ctl->info) &&
		    (int_val - snd_ctl_elem_info_get_min(ctl->info) %
		     snd_ctl_elem_info_get_step(ctl->info))) {
			ksft_print_msg("%s.%d value %ld invalid for step %ld minimum %ld\n",
				       ctl->name, index, int_val,
				       snd_ctl_elem_info_get_step(ctl->info),
				       snd_ctl_elem_info_get_min(ctl->info));
			return false;
		}
		break;

	case SND_CTL_ELEM_TYPE_INTEGER64:
		int64_val = snd_ctl_elem_value_get_integer64(val, index);

		if (int64_val < snd_ctl_elem_info_get_min64(ctl->info)) {
			ksft_print_msg("%s.%d value %lld less than minimum %lld\n",
				       ctl->name, index, int64_val,
				       snd_ctl_elem_info_get_min64(ctl->info));
			return false;
		}

		if (int64_val > snd_ctl_elem_info_get_max64(ctl->info)) {
			ksft_print_msg("%s.%d value %lld more than maximum %lld\n",
				       ctl->name, index, int64_val,
				       snd_ctl_elem_info_get_max(ctl->info));
			return false;
		}

		/* Only check step size if there is one and we're in bounds */
		if (snd_ctl_elem_info_get_step64(ctl->info) &&
		    (int64_val - snd_ctl_elem_info_get_min64(ctl->info)) %
		    snd_ctl_elem_info_get_step64(ctl->info)) {
			ksft_print_msg("%s.%d value %lld invalid for step %lld minimum %lld\n",
				       ctl->name, index, int64_val,
				       snd_ctl_elem_info_get_step64(ctl->info),
				       snd_ctl_elem_info_get_min64(ctl->info));
			return false;
		}
		break;

	case SND_CTL_ELEM_TYPE_ENUMERATED:
		int_val = snd_ctl_elem_value_get_enumerated(val, index);

		if (int_val < 0) {
			ksft_print_msg("%s.%d negative value %ld for enumeration\n",
				       ctl->name, index, int_val);
			return false;
		}

		if (int_val >= snd_ctl_elem_info_get_items(ctl->info)) {
			ksft_print_msg("%s.%d value %ld more than item count %ld\n",
				       ctl->name, index, int_val,
				       snd_ctl_elem_info_get_items(ctl->info));
			return false;
		}
		break;

	default:
		/* No tests for other types */
		break;
	}

	return true;
}

/*
 * Check that the provided value meets the constraints for the
 * provided control.
 */
bool ctl_value_valid(struct ctl_data *ctl, snd_ctl_elem_value_t *val)
{
	int i;
	bool valid = true;

	for (i = 0; i < snd_ctl_elem_info_get_count(ctl->info); i++)
		if (!ctl_value_index_valid(ctl, val, i))
			valid = false;

	return valid;
}

/*
 * Check that we can read the default value and it is valid. Write
 * tests use the read value to restore the default.
 */
void test_ctl_get_value(struct ctl_data *ctl)
{
	int err;

	/* If the control is turned off let's be polite */
	if (snd_ctl_elem_info_is_inactive(ctl->info)) {
		ksft_print_msg("%s is inactive\n", ctl->name);
		ksft_test_result_skip("get_value.%d.%d\n",
				      ctl->card->card, ctl->elem);
		return;
	}

	/* Can't test reading on an unreadable control */
	if (!snd_ctl_elem_info_is_readable(ctl->info)) {
		ksft_print_msg("%s is not readable\n", ctl->name);
		ksft_test_result_skip("get_value.%d.%d\n",
				      ctl->card->card, ctl->elem);
		return;
	}

	err = snd_ctl_elem_read(ctl->card->handle, ctl->def_val);
	if (err < 0) {
		ksft_print_msg("snd_ctl_elem_read() failed: %s\n",
			       snd_strerror(err));
		goto out;
	}

	if (!ctl_value_valid(ctl, ctl->def_val))
		err = -EINVAL;

out:
	ksft_test_result(err >= 0, "get_value.%d.%d\n",
			 ctl->card->card, ctl->elem);
}

bool show_mismatch(struct ctl_data *ctl, int index,
		   snd_ctl_elem_value_t *read_val,
		   snd_ctl_elem_value_t *expected_val)
{
	long long expected_int, read_int;

	/*
	 * We factor out the code to compare values representable as
	 * integers, ensure that check doesn't log otherwise.
	 */
	expected_int = 0;
	read_int = 0;

	switch (snd_ctl_elem_info_get_type(ctl->info)) {
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		expected_int = snd_ctl_elem_value_get_boolean(expected_val,
							      index);
		read_int = snd_ctl_elem_value_get_boolean(read_val, index);
		break;

	case SND_CTL_ELEM_TYPE_INTEGER:
		expected_int = snd_ctl_elem_value_get_integer(expected_val,
							      index);
		read_int = snd_ctl_elem_value_get_integer(read_val, index);
		break;

	case SND_CTL_ELEM_TYPE_INTEGER64:
		expected_int = snd_ctl_elem_value_get_integer64(expected_val,
								index);
		read_int = snd_ctl_elem_value_get_integer64(read_val,
							    index);
		break;

	case SND_CTL_ELEM_TYPE_ENUMERATED:
		expected_int = snd_ctl_elem_value_get_enumerated(expected_val,
								 index);
		read_int = snd_ctl_elem_value_get_enumerated(read_val,
							     index);
		break;

	default:
		break;
	}

	if (expected_int != read_int) {
		/*
		 * NOTE: The volatile attribute means that the hardware
		 * can voluntarily change the state of control element
		 * independent of any operation by software.  
		 */
		bool is_volatile = snd_ctl_elem_info_is_volatile(ctl->info);
		ksft_print_msg("%s.%d expected %lld but read %lld, is_volatile %d\n",
			       ctl->name, index, expected_int, read_int, is_volatile);
		return !is_volatile;
	} else {
		return false;
	}
}

/*
 * Write a value then if possible verify that we get the expected
 * result.  An optional expected value can be provided if we expect
 * the write to fail, for verifying that invalid writes don't corrupt
 * anything.
 */
int write_and_verify(struct ctl_data *ctl,
		     snd_ctl_elem_value_t *write_val,
		     snd_ctl_elem_value_t *expected_val)
{
	int err, i;
	bool error_expected, mismatch_shown;
	snd_ctl_elem_value_t *read_val, *w_val;
	snd_ctl_elem_value_alloca(&read_val);
	snd_ctl_elem_value_alloca(&w_val);

	/*
	 * We need to copy the write value since writing can modify
	 * the value which causes surprises, and allocate an expected
	 * value if we expect to read back what we wrote.
	 */
	snd_ctl_elem_value_copy(w_val, write_val);
	if (expected_val) {
		error_expected = true;
	} else {
		error_expected = false;
		snd_ctl_elem_value_alloca(&expected_val);
		snd_ctl_elem_value_copy(expected_val, write_val);
	}

	/*
	 * Do the write, if we have an expected value ignore the error
	 * and carry on to validate the expected value.
	 */
	err = snd_ctl_elem_write(ctl->card->handle, w_val);
	if (err < 0 && !error_expected) {
		ksft_print_msg("snd_ctl_elem_write() failed: %s\n",
			       snd_strerror(err));
		return err;
	}

	/* Can we do the verification part? */
	if (!snd_ctl_elem_info_is_readable(ctl->info))
		return err;

	snd_ctl_elem_value_set_id(read_val, ctl->id);

	err = snd_ctl_elem_read(ctl->card->handle, read_val);
	if (err < 0) {
		ksft_print_msg("snd_ctl_elem_read() failed: %s\n",
			       snd_strerror(err));
		return err;
	}

	/*
	 * Use the libray to compare values, if there's a mismatch
	 * carry on and try to provide a more useful diagnostic than
	 * just "mismatch".
	 */
	if (!snd_ctl_elem_value_compare(expected_val, read_val))
		return 0;

	mismatch_shown = false;
	for (i = 0; i < snd_ctl_elem_info_get_count(ctl->info); i++)
		if (show_mismatch(ctl, i, read_val, expected_val))
			mismatch_shown = true;

	if (!mismatch_shown)
		ksft_print_msg("%s read and written values differ\n",
			       ctl->name);

	return -1;
}

/*
 * Make sure we can write the default value back to the control, this
 * should validate that at least some write works.
 */
void test_ctl_write_default(struct ctl_data *ctl)
{
	int err;

	/* If the control is turned off let's be polite */
	if (snd_ctl_elem_info_is_inactive(ctl->info)) {
		ksft_print_msg("%s is inactive\n", ctl->name);
		ksft_test_result_skip("write_default.%d.%d\n",
				      ctl->card->card, ctl->elem);
		return;
	}

	if (!snd_ctl_elem_info_is_writable(ctl->info)) {
		ksft_print_msg("%s is not writeable\n", ctl->name);
		ksft_test_result_skip("write_default.%d.%d\n",
				      ctl->card->card, ctl->elem);
		return;
	}

	/* No idea what the default was for unreadable controls */
	if (!snd_ctl_elem_info_is_readable(ctl->info)) {
		ksft_print_msg("%s couldn't read default\n", ctl->name);
		ksft_test_result_skip("write_default.%d.%d\n",
				      ctl->card->card, ctl->elem);
		return;
	}

	err = write_and_verify(ctl, ctl->def_val, NULL);

	ksft_test_result(err >= 0, "write_default.%d.%d\n",
			 ctl->card->card, ctl->elem);
}

bool test_ctl_write_valid_boolean(struct ctl_data *ctl)
{
	int err, i, j;
	bool fail = false;
	snd_ctl_elem_value_t *val;
	snd_ctl_elem_value_alloca(&val);

	snd_ctl_elem_value_set_id(val, ctl->id);

	for (i = 0; i < snd_ctl_elem_info_get_count(ctl->info); i++) {
		for (j = 0; j < 2; j++) {
			snd_ctl_elem_value_set_boolean(val, i, j);
			err = write_and_verify(ctl, val, NULL);
			if (err != 0)
				fail = true;
		}
	}

	return !fail;
}

bool test_ctl_write_valid_integer(struct ctl_data *ctl)
{
	int err;
	int i;
	long j, step;
	bool fail = false;
	snd_ctl_elem_value_t *val;
	snd_ctl_elem_value_alloca(&val);

	snd_ctl_elem_value_set_id(val, ctl->id);

	step = snd_ctl_elem_info_get_step(ctl->info);
	if (!step)
		step = 1;

	for (i = 0; i < snd_ctl_elem_info_get_count(ctl->info); i++) {
		for (j = snd_ctl_elem_info_get_min(ctl->info);
		     j <= snd_ctl_elem_info_get_max(ctl->info); j += step) {

			snd_ctl_elem_value_set_integer(val, i, j);
			err = write_and_verify(ctl, val, NULL);
			if (err != 0)
				fail = true;
		}
	}


	return !fail;
}

bool test_ctl_write_valid_integer64(struct ctl_data *ctl)
{
	int err, i;
	long long j, step;
	bool fail = false;
	snd_ctl_elem_value_t *val;
	snd_ctl_elem_value_alloca(&val);

	snd_ctl_elem_value_set_id(val, ctl->id);

	step = snd_ctl_elem_info_get_step64(ctl->info);
	if (!step)
		step = 1;

	for (i = 0; i < snd_ctl_elem_info_get_count(ctl->info); i++) {
		for (j = snd_ctl_elem_info_get_min64(ctl->info);
		     j <= snd_ctl_elem_info_get_max64(ctl->info); j += step) {

			snd_ctl_elem_value_set_integer64(val, i, j);
			err = write_and_verify(ctl, val, NULL);
			if (err != 0)
				fail = true;
		}
	}

	return !fail;
}

bool test_ctl_write_valid_enumerated(struct ctl_data *ctl)
{
	int err, i, j;
	bool fail = false;
	snd_ctl_elem_value_t *val;
	snd_ctl_elem_value_alloca(&val);

	snd_ctl_elem_value_set_id(val, ctl->id);

	for (i = 0; i < snd_ctl_elem_info_get_count(ctl->info); i++) {
		for (j = 0; j < snd_ctl_elem_info_get_items(ctl->info); j++) {
			snd_ctl_elem_value_set_enumerated(val, i, j);
			err = write_and_verify(ctl, val, NULL);
			if (err != 0)
				fail = true;
		}
	}

	return !fail;
}

void test_ctl_write_valid(struct ctl_data *ctl)
{
	bool pass;
	int err;

	/* If the control is turned off let's be polite */
	if (snd_ctl_elem_info_is_inactive(ctl->info)) {
		ksft_print_msg("%s is inactive\n", ctl->name);
		ksft_test_result_skip("write_valid.%d.%d\n",
				      ctl->card->card, ctl->elem);
		return;
	}

	if (!snd_ctl_elem_info_is_writable(ctl->info)) {
		ksft_print_msg("%s is not writeable\n", ctl->name);
		ksft_test_result_skip("write_valid.%d.%d\n",
				      ctl->card->card, ctl->elem);
		return;
	}

	switch (snd_ctl_elem_info_get_type(ctl->info)) {
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		pass = test_ctl_write_valid_boolean(ctl);
		break;

	case SND_CTL_ELEM_TYPE_INTEGER:
		pass = test_ctl_write_valid_integer(ctl);
		break;

	case SND_CTL_ELEM_TYPE_INTEGER64:
		pass = test_ctl_write_valid_integer64(ctl);
		break;

	case SND_CTL_ELEM_TYPE_ENUMERATED:
		pass = test_ctl_write_valid_enumerated(ctl);
		break;

	default:
		/* No tests for this yet */
		ksft_test_result_skip("write_valid.%d.%d\n",
				      ctl->card->card, ctl->elem);
		return;
	}

	/* Restore the default value to minimise disruption */
	err = write_and_verify(ctl, ctl->def_val, NULL);
	if (err < 0)
		pass = false;

	ksft_test_result(pass, "write_valid.%d.%d\n",
			 ctl->card->card, ctl->elem);
}

bool test_ctl_write_invalid_value(struct ctl_data *ctl,
				  snd_ctl_elem_value_t *val)
{
	int err;
	long val_read;

	/* Ideally this will fail... */
	err = snd_ctl_elem_write(ctl->card->handle, val);
	if (err < 0)
		return false;

	/* ...but some devices will clamp to an in range value */
	err = snd_ctl_elem_read(ctl->card->handle, val);
	if (err < 0) {
		ksft_print_msg("%s failed to read: %s\n",
			       ctl->name, snd_strerror(err));
		return true;
	}

	return !ctl_value_valid(ctl, val);
}

bool test_ctl_write_invalid_boolean(struct ctl_data *ctl)
{
	int err, i;
	long val_read;
	bool fail = false;
	snd_ctl_elem_value_t *val;
	snd_ctl_elem_value_alloca(&val);

	for (i = 0; i < snd_ctl_elem_info_get_count(ctl->info); i++) {
		snd_ctl_elem_value_copy(val, ctl->def_val);
		snd_ctl_elem_value_set_boolean(val, i, 2);

		if (test_ctl_write_invalid_value(ctl, val))
			fail = true;
	}

	return !fail;
}

bool test_ctl_write_invalid_integer(struct ctl_data *ctl)
{
	int i;
	bool fail = false;
	snd_ctl_elem_value_t *val;
	snd_ctl_elem_value_alloca(&val);

	for (i = 0; i < snd_ctl_elem_info_get_count(ctl->info); i++) {
		if (snd_ctl_elem_info_get_min(ctl->info) != LONG_MIN) {
			/* Just under range */
			snd_ctl_elem_value_copy(val, ctl->def_val);
			snd_ctl_elem_value_set_integer(val, i,
			       snd_ctl_elem_info_get_min(ctl->info) - 1);

			if (test_ctl_write_invalid_value(ctl, val))
				fail = true;

			/* Minimum representable value */
			snd_ctl_elem_value_copy(val, ctl->def_val);
			snd_ctl_elem_value_set_integer(val, i, LONG_MIN);

			if (test_ctl_write_invalid_value(ctl, val))
				fail = true;
		}

		if (snd_ctl_elem_info_get_max(ctl->info) != LONG_MAX) {
			/* Just over range */
			snd_ctl_elem_value_copy(val, ctl->def_val);
			snd_ctl_elem_value_set_integer(val, i,
			       snd_ctl_elem_info_get_max(ctl->info) + 1);

			if (test_ctl_write_invalid_value(ctl, val))
				fail = true;

			/* Maximum representable value */
			snd_ctl_elem_value_copy(val, ctl->def_val);
			snd_ctl_elem_value_set_integer(val, i, LONG_MAX);

			if (test_ctl_write_invalid_value(ctl, val))
				fail = true;
		}
	}

	return !fail;
}

bool test_ctl_write_invalid_integer64(struct ctl_data *ctl)
{
	int i;
	bool fail = false;
	snd_ctl_elem_value_t *val;
	snd_ctl_elem_value_alloca(&val);

	for (i = 0; i < snd_ctl_elem_info_get_count(ctl->info); i++) {
		if (snd_ctl_elem_info_get_min64(ctl->info) != LLONG_MIN) {
			/* Just under range */
			snd_ctl_elem_value_copy(val, ctl->def_val);
			snd_ctl_elem_value_set_integer64(val, i,
				snd_ctl_elem_info_get_min64(ctl->info) - 1);

			if (test_ctl_write_invalid_value(ctl, val))
				fail = true;

			/* Minimum representable value */
			snd_ctl_elem_value_copy(val, ctl->def_val);
			snd_ctl_elem_value_set_integer64(val, i, LLONG_MIN);

			if (test_ctl_write_invalid_value(ctl, val))
				fail = true;
		}

		if (snd_ctl_elem_info_get_max64(ctl->info) != LLONG_MAX) {
			/* Just over range */
			snd_ctl_elem_value_copy(val, ctl->def_val);
			snd_ctl_elem_value_set_integer64(val, i,
				snd_ctl_elem_info_get_max64(ctl->info) + 1);

			if (test_ctl_write_invalid_value(ctl, val))
				fail = true;

			/* Maximum representable value */
			snd_ctl_elem_value_copy(val, ctl->def_val);
			snd_ctl_elem_value_set_integer64(val, i, LLONG_MAX);

			if (test_ctl_write_invalid_value(ctl, val))
				fail = true;
		}
	}

	return !fail;
}

bool test_ctl_write_invalid_enumerated(struct ctl_data *ctl)
{
	int err, i;
	unsigned int val_read;
	bool fail = false;
	snd_ctl_elem_value_t *val;
	snd_ctl_elem_value_alloca(&val);

	snd_ctl_elem_value_set_id(val, ctl->id);

	for (i = 0; i < snd_ctl_elem_info_get_count(ctl->info); i++) {
		/* One beyond maximum */
		snd_ctl_elem_value_copy(val, ctl->def_val);
		snd_ctl_elem_value_set_enumerated(val, i,
				  snd_ctl_elem_info_get_items(ctl->info));

		if (test_ctl_write_invalid_value(ctl, val))
			fail = true;

		/* Maximum representable value */
		snd_ctl_elem_value_copy(val, ctl->def_val);
		snd_ctl_elem_value_set_enumerated(val, i, UINT_MAX);

		if (test_ctl_write_invalid_value(ctl, val))
			fail = true;

	}

	return !fail;
}


void test_ctl_write_invalid(struct ctl_data *ctl)
{
	bool pass;
	int err;

	/* If the control is turned off let's be polite */
	if (snd_ctl_elem_info_is_inactive(ctl->info)) {
		ksft_print_msg("%s is inactive\n", ctl->name);
		ksft_test_result_skip("write_invalid.%d.%d\n",
				      ctl->card->card, ctl->elem);
		return;
	}

	if (!snd_ctl_elem_info_is_writable(ctl->info)) {
		ksft_print_msg("%s is not writeable\n", ctl->name);
		ksft_test_result_skip("write_invalid.%d.%d\n",
				      ctl->card->card, ctl->elem);
		return;
	}

	switch (snd_ctl_elem_info_get_type(ctl->info)) {
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		pass = test_ctl_write_invalid_boolean(ctl);
		break;

	case SND_CTL_ELEM_TYPE_INTEGER:
		pass = test_ctl_write_invalid_integer(ctl);
		break;

	case SND_CTL_ELEM_TYPE_INTEGER64:
		pass = test_ctl_write_invalid_integer64(ctl);
		break;

	case SND_CTL_ELEM_TYPE_ENUMERATED:
		pass = test_ctl_write_invalid_enumerated(ctl);
		break;

	default:
		/* No tests for this yet */
		ksft_test_result_skip("write_invalid.%d.%d\n",
				      ctl->card->card, ctl->elem);
		return;
	}

	/* Restore the default value to minimise disruption */
	err = write_and_verify(ctl, ctl->def_val, NULL);
	if (err < 0)
		pass = false;

	ksft_test_result(pass, "write_invalid.%d.%d\n",
			 ctl->card->card, ctl->elem);
}

int main(void)
{
	struct ctl_data *ctl;

	ksft_print_header();

	find_controls();

	ksft_set_plan(num_controls * TESTS_PER_CONTROL);

	for (ctl = ctl_list; ctl != NULL; ctl = ctl->next) {
		/*
		 * Must test get_value() before we write anything, the
		 * test stores the default value for later cleanup.
		 */
		test_ctl_get_value(ctl);
		test_ctl_write_default(ctl);
		test_ctl_write_valid(ctl);
		test_ctl_write_invalid(ctl);
	}

	ksft_exit_pass();

	return 0;
}
