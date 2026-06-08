import time
import re
import os

LOG_FILE = "/data/proxy.log"

# Define Security Rules (YARA/IDS logic)
RULES = [
    {"name": "XSS_ATTACK", "pattern": re.compile(r"<script>.*?</script>", re.IGNORECASE)},
    {"name": "SQL_INJECTION", "pattern": re.compile(r"DROP\s+TABLE|OR\s+1=1", re.IGNORECASE)},
    {"name": "PATH_TRAVERSAL", "pattern": re.compile(r"\.\./\.\./|/etc/passwd|/etc/shadow", re.IGNORECASE)},
    {"name": "COMMAND_INJECTION", "pattern": re.compile(r"\|\|?\s*ls|&&\s*cat|\$\(.*\)", re.IGNORECASE)}
]

def scan_log_line(line):
    for rule in RULES:
        if rule["pattern"].search(line):
            print("\n" + "="*60)
            print("[SIEM ALERT] MALICIOUS PAYLOAD DETECTED!")
            print(f"Rule Matched: {rule['name']}")
            print(f"Log Details: {line.strip()}")
            print("="*60 + "\n")

def main():
    print("[*] Security Scanner (IDS/SIEM) Started. Tailing proxy.log...")
    
    while not os.path.exists(LOG_FILE):
        time.sleep(1)
        
    with open(LOG_FILE, "r") as f:
        f.seek(0, 2) # Tail from end
        while True:
            line = f.readline()
            if not line:
                time.sleep(0.5)
                continue
            scan_log_line(line)

if __name__ == "__main__":
    main()
