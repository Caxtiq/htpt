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
        data = s.recv(1024)
        s.close()
        return data.decode().strip()
    except Exception as e:
        return f"ERROR: {e}"

def put(key, value):
    print(f"\n=> Sending PUT {key}='{value}' to PROXY")
    resp = send_tcp(f"PUT {key} {value}\n")
    print(f"Response: {resp}")

def get(key):
    print(f"\n=> Sending GET key '{key}' to PROXY")
    resp = send_tcp(f"GET {key}\n")
    print(f"Response: {resp}")

if __name__ == "__main__":
    print("=== STARTING TEST WITH TCP PROXY LOAD BALANCER ===")
    
    put("course", "Distributed Systems")
    put("university", "owo")
    get("course")
    
    print("\n=======================================================")
    print("SECURITY DEMO (WAF/IDS TRIGGER):")
    print("Sending malicious payloads to test the Security Scanner...")
    time.sleep(1)
    
    put("hacked_key", "<script>alert('XSS')</script>")
    put("sql_injection", "DROP TABLE users;")
    put("path_trav", "../../../etc/passwd")
    
    print("\n[+] Check the Docker logs of 'kv_security_scanner' to see the ALERTS!")
    print("Command: docker logs -f kv_security_scanner")
    print("=======================================================")
