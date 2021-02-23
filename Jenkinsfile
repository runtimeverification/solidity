pipeline {
  agent {
    dockerfile {
      label 'docker'
      additionalBuildArgs '--build-arg KIELE_COMMIT=$(cat ext/kiele_release | cut --delimiter="-" --field="2") --build-arg USER_ID=$(id -u) --build-arg GROUP_ID=$(id -g)'
    }
  }
  options { ansiColor('xterm') }
  stages {
    stage("Init title") {
      when { changeRequest() }
      steps { script { currentBuild.displayName = "PR ${env.CHANGE_ID}: ${env.CHANGE_TITLE}" } }
    }
    stage('Build') {
      steps {
        sh '''
          mkdir build
          cd build
          cmake .. -DCMAKE_BUILD_TYPE=Debug
          make -j`nproc`
          cd ..
        '''
      }
    }
    stage('Compilation Tests') {
      steps { sh './test/ieleCmdlineTests.sh' }
    }
    stage('Execution Tests') {
      steps {
        sh '''
          kiele vm --port 9001 &
          kiele_pid=$!
          sleep 3
          iele-test-client build/ipcfile 9001 &
          testnode_pid=$!
          sleep 3
          ./build/test/soltest \
            -c no \
            -e build/report.xml \
            -r detailed \
            -m XML \
            -k build/log.xml \
            -l all \
            -f XML  \
            `cat test/failing-exec-tests` \
            -- \
            --ipcpath build/ipcfile \
            --testpath test
          iconv -f iso-8859-1 \
                -t utf-8 build/log.xml \
                -o build/log-utf8.xml
          mv -f build/log-utf8.xml build/log.xml
          xmllint --xpath "//TestCase[@assertions_failed!=@expected_failures]" build/report.xml && false
          xmllint --xpath '//TestCase[@result="skipped"]' build/report.xml && false
          cd build
          gcovr -x -r .. -e ../test -o coverage.xml
          kill $testnode_pid $kiele_pid
        '''
      }
    }
  }
}
