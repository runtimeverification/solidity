# Solidity to IELE compiler docker 


<!-- @import "[TOC]" {cmd="toc" depthFrom=2 depthTo=6 orderedList=false} -->

<!-- code_chunk_output -->

* [Build docker image](#build-docker-image)
* [Start container as an HTTP server](#start-container-as-an-http-server)
* [Run container that mounts local directory](#run-container-that-mounts-local-directory)

<!-- /code_chunk_output -->


## Build docker image 

```bash
# pull from docker hub
$ docker pull runtimeverification/sol2iele:latest

# or build from Dockerfile
$ docker build -t runtimeverification/sol2iele:latest .
```

## Start container as an HTTP server

Run the following command to start the server at port `7777`:

```bash
$ docker run -ti --rm -p 7777:8080 runtimeverification/sol2iele:latest
```

Then you can compile solidity code to iele by sending a `POST` request to the server:

```bash
# Assume the container is running from the previous step
$ curl -X POST --data '{"method": "sol2iele_asm", "params": ["main.sol", {"main.sol":"code1...","./dir/dependency.sol":"code2..."}]}' localhost:7777
```


For example, assume we have two solidity files: 

```solidity
// owned.sol
pragma solidity ^0.4.9;
contract owned {
    function owned() { owner = msg.sender; }
    address owner;
}

// mortal.sol
pragma solidity ^0.4.9;
import "./owned.sol";
contract mortal is owned{
    function kill() {
        selfdestruct(owner);
    }
}
```

then we can compile this two files like below:

```bash
$ curl -X POST --data '{"method": "sol2iele_asm", "params": ["mortal.sol", {"owned.sol":"pragma solidity ^0.4.9;\ncontract owned {\n    function owned() { owner = msg.sender; }\n    address owner;\n}","mortal.sol":"pragma solidity ^0.4.9;\nimport \"./owned.sol\";\ncontract mortal is owned{\n    function kill() {\n        selfdestruct(owner);\n    }\n}"}]}' localhost:7777
```

## Run container that mounts local directory  

```bash
$ docker run -it --rm -v absolute_path_to_local_dir:/src runtimeverification/sol2iele bash
```

You will then enter the interactive mode in docker container.

```bash
$ cd /src     # this is where you mount your local directory
$ isolc --asm file_you_want_to_compile.sol  # compile a solidity file
```