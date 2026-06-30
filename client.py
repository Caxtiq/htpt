import socket
import time

PROXY_HOST = "localhost"
PROXY_PORT = 8000

def send_tcp(message):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2)
        s.connect((PROXY_HOST, PROXY_PORT))
        s.sendall(message.encode())
        
        # Read all chunks until the server closes the connection
        chunks = []
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            chunks.append(chunk.decode(errors='ignore'))
            
        s.close()
        return "".join(chunks).strip()
    except Exception as e:
        return f"ERROR: {e}"

if __name__ == "__main__":
    print("=== INTERACTIVE CLIENT SIMULATOR ===")
    print("Commands supported by Cluster:")
    print("  PUT <key> <value>")
    print("  GET <key>")
    print("  DEL <key>")
    print("  exit")
    print("====================================")
    
    while True:
        try:
            cmd = input("\nKV-Shell> ").strip()
            if cmd.lower() in ['exit', 'quit']:
                break
            if not cmd:
                continue
                
            resp = send_tcp(cmd + "\n")
            print(f"Cluster Response: {resp}")
        except KeyboardInterrupt:
            print("\nExiting...")
            break
