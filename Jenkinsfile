pipeline {
  agent any
  stages {
    stage('Clean') {
      parallel {
        stage('Clean') {
          steps {
            sh 'git reset --hard'
            sh 'make O=out clean'
          }
        }

        stage('Rm old kernel') {
          steps {
            //sh '''sudo rm -rf /boot/**5.**
//sudo rm -rf /lib/modules/**'''
          }
        }

      }
    }

    stage('Kconfig') {
      steps {
        sh 'make O=out laptop_defconfig'
      }
    }

    stage('Kbuild') {
      steps {
        sh 'make -j16 O=out'
      }
    }

    stage('Install') {
      steps {
        sh '''sudo make O=out modules_install
sudo make O=out install'''
      }
    }

  }
}
Terms
Pri
