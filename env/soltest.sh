#!/bin/bash
cd solidity
./build/test/soltest --report_sink=build/report.xml --report_level=detailed --report_format=XML --log_level=all `cat test/failing-exec-tests` `cat test/out-of-scope-exec-tests` `cat test/unimplemented-features-tests` -- --enforce-no-yul-ewasm --ipcpath ~/.midnight-wallet/midnight.ipc --testpath test --wallet-id "ce3bbd3e" --private-from-address "m-test-shl-ad1x4k38xhcz4nvjv75cz5nvs447zed9f8mpa8sat2q8j95q9twjd7ku7te3uyuz94r6km0zwrwcnl"
