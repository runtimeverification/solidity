#!/usr/bin/env python3

from http.server import BaseHTTPRequestHandler, HTTPServer
import subprocess
import json
import tempfile
import shutil
import os

# HTTPRequestHandler class
class testHTTPServer_RequestHandler(BaseHTTPRequestHandler):

    # GET
    def do_GET(self):
        # Send response status code
        self.send_response(200)

        # Send headers
        self.send_header('Content-Type','application/json')
        self.end_headers()

        # Write content as utf-8 data
        self.wfile.write(bytes('{"success":true, "data": "Server is running!"}', "utf8"))
        return
    
    def do_POST(self):
        def validateTargetFilePath(filepath, dirpath):
            realpath = os.path.realpath(filepath)
            return realpath.startswith(dirpath)

        dirpath = None
        id_ = None
        timeout = 15 # timeout in second to kill the process
        try:
            content_length = int(self.headers['Content-Length']) # <--- Gets the size of data
            post_data = self.rfile.read(content_length).decode('utf8') # <--- Gets the data itself
            post_json = json.loads(post_data)
            method = post_json.get("method", None)
            params = post_json.get("params", [])
            jsonrpc = post_json.get("jsonrpc", None)
            id_ = post_json.get("id", None)
            if jsonrpc != "2.0": # Invalid jsonrpc 
                response_json = json.dumps({"jsonrpc": "2.0", "id": id_, "error": {"code": "-32600", "message": "Invalid Request"}})
            elif method == "sol2iele_asm" or method == "sol2iele_abi":
                dirpath = tempfile.mkdtemp()
                main_filepath = params[0]
                if not validateTargetFilePath(os.path.join(dirpath, main_filepath), dirpath):
                    raise Exception("Invalid filepath " + main_filepath)
                filepath_code_map = params[1]
                for filepath in filepath_code_map:
                    code = filepath_code_map[filepath]
                    target_filepath = os.path.join(dirpath, filepath)
                    if not validateTargetFilePath(target_filepath, dirpath):
                        raise Exception("Invalid filepath " + filepath)
                    os.makedirs(os.path.dirname(target_filepath), exist_ok=True) # create directory if not exists
                    with open(target_filepath, "w") as outfile:
                        outfile.write(code)
                if method == "sol2iele_asm":
                    flag = "--asm"
                else:
                    flag = "--abi"
                p = subprocess.Popen(["isolc", flag, main_filepath], cwd=dirpath, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                (result, error) = p.communicate(timeout=timeout)
                if error != None:
                    raise Exception(error)
                else:
                    response_json = json.dumps({"jsonrpc": "2.0", "result": result.decode('utf8').replace(dirpath+'/', ''), "id": id_})
            elif method == "iele_asm":
                dirpath = tempfile.mkdtemp()
                main_filepath = params[0]
                if not validateTargetFilePath(os.path.join(dirpath, main_filepath), dirpath):
                    raise Exception("Invalid filepath " + main_filepath)
                filepath_code_map = params[1]
                for filepath in filepath_code_map:
                    code = filepath_code_map[filepath]
                    target_filepath = os.path.join(dirpath, filepath)
                    if not validateTargetFilePath(target_filepath, dirpath):
                        raise Exception("Invalid filepath " + filepath)
                    os.makedirs(os.path.dirname(target_filepath), exist_ok=True) # create directory if not exists
                    with open(target_filepath, "w") as outfile:
                        outfile.write(code)
                p = subprocess.Popen(["iele-assemble", main_filepath], cwd=dirpath, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                (result, error) = p.communicate(timeout=timeout)
                if error != None:
                    raise Exception(error)
                else:
                    response_json = json.dumps({"jsonrpc":"2.0", "result": result.decode('utf8').replace(dirpath+'/', ''), "id": id_})
            else: # invalid method
                response_json = json.dumps({"jsonrpc": "2.0", "id": id_, "error": {"code": "-32601", "message": "Method not found", "data": method}})

            if dirpath != None:
                shutil.rmtree(dirpath)
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(bytes(response_json, 'utf8'))
        except Exception as error:
            if dirpath != None:
                shutil.rmtree(dirpath)
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(bytes(json.dumps({"jsonrpc": "2.0","id": id_,"error":{"code": "-32603", "message": "Internal error", "data": repr(error)}}), 'utf8'))
def run():
    print('starting server...')
    HOST = '0.0.0.0' # '127.0.0.1'
    PORT = os.environ.get("SERVER_PORT")
    if PORT == None:
        PORT = 8080
    else:
        PORT = int(PORT)
    # Server settings
    server_address = (HOST, PORT)
    httpd = HTTPServer(server_address, testHTTPServer_RequestHandler)
    print('running server...')
    httpd.serve_forever()

run()