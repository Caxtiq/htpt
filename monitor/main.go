package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"regexp"
	"strings"
	"sync"
	"time"
)

type LogEntry struct {
	Timestamp string `json:"timestamp"`
	Src       string `json:"src"`
	Target    string `json:"target"`
	Payload   string `json:"payload"`
}

type Alert struct {
	Timestamp string `json:"timestamp"`
	Rule      string `json:"rule"`
	Payload   string `json:"payload"`
}

type ChecklistItem struct {
	ID     int    `json:"id"`
	Task   string `json:"task"`
	IsDone bool   `json:"is_done"`
}

type Stats struct {
	TotalRequests int `json:"total_requests"`
	TotalAlerts   int `json:"total_alerts"`
}

var (
	logs        []LogEntry
	alerts      []Alert
	checklist   []ChecklistItem
	stats       Stats
	taskCounter int
	mu          sync.RWMutex
)

// SIEM Security Rules (Regex Engine)
var rules = map[string]*regexp.Regexp{
	"XSS_ATTACK":        regexp.MustCompile(`(?i)<script>.*?</script>`),
	"SQL_INJECTION":     regexp.MustCompile(`(?i)DROP\s+TABLE|OR\s+1=1`),
	"PATH_TRAVERSAL":    regexp.MustCompile(`(?i)\.\./\.\./|/etc/passwd|/etc/shadow`),
	"COMMAND_INJECTION": regexp.MustCompile(`(?i)\|\|?\s*ls|&&\s*cat|\$\(.*?\)`),
}

func parseLogLine(line string) {
	parts := strings.Split(line, " | ")
	if len(parts) >= 4 {
		timestamp := parts[0]
		src := strings.TrimPrefix(parts[1], "SRC:")
		target := strings.TrimPrefix(parts[2], "TARGET:")
		payload := strings.TrimPrefix(parts[3], "PAYLOAD: ")

		entry := LogEntry{Timestamp: timestamp, Src: src, Target: target, Payload: payload}

		mu.Lock()
		logs = append([]LogEntry{entry}, logs...)
		if len(logs) > 100 {
			logs = logs[:100]
		}
		stats.TotalRequests++

		// Scan for Threats
		for ruleName, regex := range rules {
			if regex.MatchString(payload) {
				alert := Alert{Timestamp: timestamp, Rule: ruleName, Payload: payload}
				alerts = append([]Alert{alert}, alerts...)
				if len(alerts) > 50 {
					alerts = alerts[:50]
				}
				stats.TotalAlerts++
				break
			}
		}
		mu.Unlock()
	}
}

func tailLogs() {
	logFile := "/data/proxy.log"
	var file *os.File
	var err error

	for {
		file, err = os.Open(logFile)
		if err == nil {
			break
		}
		time.Sleep(1 * time.Second)
	}
	defer file.Close()

	file.Seek(0, 2)
	reader := bufio.NewReader(file)

	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			time.Sleep(500 * time.Millisecond)
			continue
		}
		line = strings.TrimSpace(line)
		if line != "" {
			parseLogLine(line)
		}
	}
}

func corsMiddleware(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type")
		if r.Method == "OPTIONS" {
			w.WriteHeader(http.StatusOK)
			return
		}
		next(w, r)
	}
}

func apiHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	// Checklist actions
	if r.URL.Path == "/api/checklist" {
		if r.Method == "GET" {
			mu.RLock()
			json.NewEncoder(w).Encode(checklist)
			mu.RUnlock()
			return
		} else if r.Method == "POST" {
			var req struct {
				Action string `json:"action"`
				Task   string `json:"task"`
				ID     int    `json:"id"`
			}
			json.NewDecoder(r.Body).Decode(&req)

			mu.Lock()
			if req.Action == "add" {
				taskCounter++
				checklist = append(checklist, ChecklistItem{ID: taskCounter, Task: req.Task, IsDone: false})
			} else if req.Action == "toggle" {
				for i, item := range checklist {
					if item.ID == req.ID {
						checklist[i].IsDone = !checklist[i].IsDone
						break
					}
				}
			} else if req.Action == "delete" {
				var newChecklist []ChecklistItem
				for _, item := range checklist {
					if item.ID != req.ID {
						newChecklist = append(newChecklist, item)
					}
				}
				checklist = newChecklist
			}
			mu.Unlock()
			json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
			return
		}
	}

	mu.Lock()
	defer mu.Unlock()

	// Handle GET & DELETE for Logs and Alerts
	if r.URL.Path == "/api/logs" {
		if r.Method == "DELETE" {
			logs = []LogEntry{}
			json.NewEncoder(w).Encode(map[string]string{"status": "cleared"})
		} else {
			json.NewEncoder(w).Encode(logs)
		}
	} else if r.URL.Path == "/api/alerts" {
		if r.Method == "DELETE" {
			alerts = []Alert{}
			json.NewEncoder(w).Encode(map[string]string{"status": "cleared"})
		} else {
			json.NewEncoder(w).Encode(alerts)
		}
	} else if r.URL.Path == "/api/stats" {
		if r.Method == "DELETE" {
			stats.TotalRequests = 0
			stats.TotalAlerts = 0
			json.NewEncoder(w).Encode(map[string]string{"status": "cleared"})
		} else {
			json.NewEncoder(w).Encode(stats)
		}
	} else {
		http.NotFound(w, r)
	}
}

func main() {
	go tailLogs()

	http.HandleFunc("/api/", corsMiddleware(apiHandler))

	fmt.Println("[*] Golang SIEM Bridge serving API on :9000")
	log.Fatal(http.ListenAndServe(":9000", nil))
}
