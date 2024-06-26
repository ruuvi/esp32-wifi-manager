#!/bin/bash

set -e # Exit script on non-zero command exit status
set -x # Print commands and their arguments as they are executed.

export SONAR_TOKEN=$SONAR_TOKEN_esp32_wifi_manager
if [ "$SONAR_TOKEN" = "" ]; then
     echo Environment variable "SONAR_TOKEN_esp32_wifi_manager" is not set
     exit 1
fi

mkdir -p "$HOME/bin"
if [ ! -d "$HOME/bin/sonar-scanner" ]; then
  rm -f "$HOME/bin/sonar-scanner-cli-4.7.0.2747-linux.zip"
  wget -P "$HOME/bin" "https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-4.7.0.2747-linux.zip"
  unzip -q -o "$HOME/bin/sonar-scanner-cli-4.7.0.2747-linux.zip" -d "$HOME/bin"
  mv "$HOME/bin/sonar-scanner-4.7.0.2747-linux" "$HOME/bin/sonar-scanner"
fi
export PATH=$PATH:$HOME/bin/sonar-scanner/bin

rm -f "$HOME/bin/build-wrapper-linux-x86.zip"
wget -P "$HOME/bin" https://sonarcloud.io/static/cpp/build-wrapper-linux-x86.zip
unzip -o "$HOME/bin/build-wrapper-linux-x86.zip" -d "$HOME/bin"
export PATH=$PATH:$HOME/bin/build-wrapper-linux-x86

echo Initial cleanup
find . -type f -name "*.gcno" -exec rm -f {} \;
find . -type f -name '*.gcna' -exec rm -f {} \;
find . -type f -name "*.gcov" -exec rm -f {} \;
find . -type f -name 'gtestresults.xml' -exec rm -f {} \;
rm -f coverage.xml

cd examples/default_demo
rm -rf build
mkdir -p build
cd build
which build-wrapper-linux-x86-64
build-wrapper-linux-x86-64 --out-dir ./bw-output cmake -DCMAKE_BUILD_TYPE=Debug -G "Ninja" ..
build-wrapper-linux-x86-64 --out-dir ./bw-output ninja
cd ../../..

cd tests
rm -rf build
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage" -G "Ninja" ..
ninja
ctest
cd ../..
gcovr -r . --sonarqube -o coverage.xml

which sonar-scanner
sonar-scanner --version
sonar-scanner -X \
  --define sonar.cfamily.build-wrapper-output="examples/default_demo/build/bw-output" \
  --define sonar.host.url=https://sonarcloud.io \
  --define sonar.coverageReportPaths=coverage.xml \
  --define sonar.cfamily.threads=$(nproc)

echo Final cleanup
rm -f coverage.xml
rm -rf examples/default_demo/build
rm -rf tests/build
find . -type f -name "*.gcno" -exec rm -f {} \;
find . -type f -name '*.gcna' -exec rm -f {} \;
find . -type f -name "*.gcov" -exec rm -f {} \;
find . -type f -name 'gtestresults.xml' -exec rm -f {} \;
