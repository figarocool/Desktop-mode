import http.server,threading,subprocess,sys
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  if self.path=='/redirect':
   self.send_response(302);self.send_header('Location','/page');self.end_headers();return
  body=('<html><head><meta charset="utf-8"></head><body><p>Caffè &amp; tè</p><script>SECRET_SCRIPT</script><a href="/next">Pagina successiva</a></body></html>' if self.path=='/page' else '<p>Pagina successiva caricata</p>').encode()
  self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:subprocess.run([sys.argv[1] if len(sys.argv)>1 else 'native/build/browser-test',f'http://127.0.0.1:{server.server_port}/redirect'],check=True)
finally:server.shutdown();server.server_close()
