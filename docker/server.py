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
        dirpath = None
        try:
            content_length = int(self.headers['Content-Length']) # <--- Gets the size of data
            post_data = self.rfile.read(content_length).decode('utf8') # <--- Gets the data itself
            post_json = json.loads(post_data)
            method = post_json["method"]
            params = post_json["params"]
            if method == "sol2iele_asm":
                dirpath = tempfile.mkdtemp()
                main_file_path = params[0]
                filepath_code_map = params[1]
                for filepath in filepath_code_map:
                    code = filepath_code_map[filepath]
                    target_filename = os.path.join(dirpath, filepath)
                    os.makedirs(os.path.dirname(target_filename), exist_ok=True) # create directory if not exists
                    with open(target_filename, "w") as outfile:
                        outfile.write(code)
                p = subprocess.Popen(["isolc", "--asm", os.path.join(dirpath, main_file_path)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                (result, error) = p.communicate()
                if error != None:
                    raise Exception(error)
                else:
                    response_json = json.dumps({"success":True, "data": result.decode('utf8').replace(dirpath+'/', '')})
            elif method == "iele_asm":
                dirpath = tempfile.mkdtemp()
                main_file_path = params[0]
                filepath_code_map = params[1]
                for filepath in filepath_code_map:
                    code = filepath_code_map[filepath]
                    target_filename = os.path.join(dirpath, filepath)
                    os.makedirs(os.path.dirname(target_filename), exist_ok=True) # create directory if not exists
                    with open(target_filename, "w") as outfile:
                        outfile.write(code)
                p = subprocess.Popen(["iele-assemble", os.path.join(dirpath, main_file_path)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                (result, error) = p.communicate()
                if error != None:
                    raise Exception(error)
                else:
                    response_json = json.dumps({"success":True, "data": result.decode('utf8').replace(dirpath+'/', '')})                
            else:
                raise Exception("Invalid method %s" % (method))

            if dirpath != None:
                shutil.rmtree(dirpath)
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()
            self.wfile.write(bytes(response_json, 'utf8'))
        except Exception as error:
            if dirpath != None:
                shutil.rmtree(dirpath)
            self.send_response(500)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(bytes('{"success": false, "data": "%s"}' % (repr(error)), 'utf8'))
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