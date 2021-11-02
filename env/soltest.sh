#!/bin/bash
cd solidity
./build/test/soltest --report_sink=build/report.xml --report_level=detailed --report_format=XML --log_level=all `cat test/failing-exec-tests` `cat test/out-of-scope-exec-tests` `cat test/unimplemented-features-tests` -- --enforce-no-yul-ewasm --ipcpath ~/.midnight-wallet/midnight.ipc --testpath test --wallet-id "ce3bbd3e" --private-from-address "m-test-shl-ad15lx6zqyg9dw7zjwlhq32wvza6f548upx6x74d8lfclr7s9u9vh853ua3eyhsd5xgeuywu9l6g39"
