import socket
import threading
import itertools
import logging

nodes = [("node1", 8001), ("node2", 8002), ("node3", 8003)]
node_iter = itertools.cycle(nodes)

logging.basicConfig(filename='/data/proxy.log', level=logging.INFO, format='%(asctime)s | %(message)s')

def handle_client(client_sock, addr):
    target_node = next(node_iter)
    
    try:
        request = client_sock.recv(4096)
        if not request:
            client_sock.close()
            return
            
        req_text = request.decode('utf-8', errors='ignore').strip()
        
        # LOG THE REQUEST FOR SIEM/IDS
        logging.info(f"SRC:{addr[0]} | TARGET:{target_node[0]} | PAYLOAD: {req_text}")
        print(f"[*] Proxying request from {addr[0]} to {target_node[0]}...")

        # Forward to C Node
        node_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        node_sock.settimeout(2)
        node_sock.connect(target_node)
        node_sock.sendall(request)
        
        response = node_sock.recv(4096)
        
        client_sock.sendall(response)
        node_sock.close()
    except Exception as e:
        print(f"[!] Error proxying to {target_node[0]}: {e}")
        try:
            client_sock.sendall(b"ERROR_PROXY_NODE_DOWN\n")
        except:
            pass
            
    client_sock.close()

def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", 8000))
    server.listen(10)
    print("[*] TCP Proxy (Load Balancer & Logger) listening on port 8000...")
    
    while True:
        client_sock, addr = server.accept()
        thread = threading.Thread(target=handle_client, args=(client_sock, addr))
        thread.start()

if __name__ == "__main__":
    main()
