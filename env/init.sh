#!/bin/bash
curl -X POST -H 'Content-Type: application/json' -d @wallet_restore.json http://localhost:8342
curl -X POST -H 'Content-Type: application/json' -d @qa_mineBlocks.json http://localhost:8342
curl -X POST -H 'Content-Type: application/json' -d @wallet_unlock.json http://localhost:8342
