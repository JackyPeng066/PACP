#!/bin/bash
# run2.sh - Goal 2 Optimizer Controller with Real-time Monitor
# Usage: ./run2.sh <L> [Workers=4] [TimeLimit=0]

L=$1
WORKERS=${2:-4}
TIME_LIMIT=${3:-0}

# --- Config ---
BINARY="./bin/optimizer2"
ROOT_DIR="./results"

# --- Colors ---
C_RESET='\033[0m'
C_RED='\033[1;31m'; C_GREEN='\033[1;32m'; C_BLUE='\033[1;34m'
C_YELLOW='\033[1;33m'; C_CYAN='\033[1;36m'; C_WHITE='\033[1;37m'
C_GRAY='\033[0;90m'

# --- 1. Validation ---
if [ -z "$L" ]; then
    echo "Usage: $0 <Length> [Workers] [TimeLimit]"
    exit 1
fi

if [ ! -f "$BINARY" ]; then
    echo -e "${C_RED}Error: Binary $BINARY not found.${C_RESET}"
    echo "Please compile manually: g++ -O3 -march=native -funroll-loops -o bin/optimizer2 src/optimizer2.cpp"
    exit 1
fi

# --- 2. Cleanup Trap ---
cleanup() {
    echo -ne "\033[?25h" # Show cursor
    echo -e "\n${C_YELLOW}Stopping all workers...${C_RESET}"
    # Kill child processes (workers)
    pkill -P $$ 2>/dev/null
    exit 0
}
trap cleanup SIGINT SIGTERM

# --- 3. Launch Workers ---
# Prepare directory (Script handles root, C++ handles subdirs)
mkdir -p "$ROOT_DIR/$L"
# Clear stale status logs to prevent reading old data
rm -f "$ROOT_DIR/$L"/worker_*/status.log

clear
echo -e "${C_CYAN}Initializing $WORKERS workers for L=$L...${C_RESET}"

for ((i=1; i<=WORKERS; i++)); do
    "$BINARY" "$L" "$ROOT_DIR" "$i" "0" "$TIME_LIMIT" > /dev/null 2>&1 &
done

# --- 4. Monitoring Loop ---
echo -ne "\033[?25l" # Hide cursor
START_TIME=$(date +%s)

while true; do
    # Move cursor to top-left (no clear to avoid flicker)
    echo -ne "\033[H"
    
    ELAPSED=$(( $(date +%s) - START_TIME ))
    
    # Draw Header
    echo -e "${C_BLUE}┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓${C_RESET}"
    printf "${C_BLUE}┃${C_WHITE} Goal 2 Runner | L=%-3d | Workers: %-2d | Time: %-5ds           ${C_BLUE}┃${C_RESET}\n" "$L" "$WORKERS" "$ELAPSED"
    echo -e "${C_BLUE}┣━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━┫${C_RESET}"
    echo -e "${C_BLUE}┃${C_WHITE} ID   ┃ Iter     ┃ Restarts ┃ Viol ┃ MidVal   ┃ Found    ┃ Time ┃${C_RESET}"
    echo -e "${C_BLUE}┣━━━━━━╋━━━━━━━━━━╋━━━━━━━━━━╋━━━━━━╋━━━━━━━━━━╋━━━━━━━━━━╋━━━━━━┫${C_RESET}"

    TOTAL_FOUND=0
    
    for ((i=1; i<=WORKERS; i++)); do
        LOG_FILE="$ROOT_DIR/$L/worker_$i/status.log"
        
        # Default placeholders
        ITER="-"; RST="-"; VIOL="-"; MID="-"; FOUND="-"; TIME="-"
        COLOR=$C_GRAY
        
        if [ -f "$LOG_FILE" ]; then
            # Read last line efficiently
            # Format: Iter: %lld | Rst: %d | ZCZ_Viol: %d | Mid: %d | Found: %lld | Time: %lds
            LINE=$(tail -n 1 "$LOG_FILE")
            
            if [ -n "$LINE" ]; then
                # Simple parsing without heavy awk/sed if possible for speed, but grep/cut is fine here
                ITER=$(echo "$LINE" | grep -o "Iter: [0-9]*" | cut -d' ' -f2)
                RST=$(echo "$LINE" | grep -o "Rst: [0-9]*" | cut -d' ' -f2)
                VIOL=$(echo "$LINE" | grep -o "ZCZ_Viol: [0-9]*" | cut -d' ' -f2)
                MID=$(echo "$LINE" | grep -o "Mid: [0-9]*" | cut -d' ' -f2)
                FOUND_VAL=$(echo "$LINE" | grep -o "Found: [0-9]*" | cut -d' ' -f2)
                TIME=$(echo "$LINE" | grep -o "Time: [0-9]*s" | cut -d' ' -f2)
                
                # Logic for status color
                if [ "$FOUND_VAL" -gt 0 ]; then
                    COLOR=$C_GREEN
                    TOTAL_FOUND=$((TOTAL_FOUND + FOUND_VAL))
                    FOUND="$FOUND_VAL"
                else
                    FOUND="0"
                    if [ "$VIOL" == "0" ]; then
                        COLOR=$C_YELLOW # ZCZ Clean! Hunting Peak...
                    elif [ "$VIOL" -lt 5 ]; then
                        COLOR=$C_CYAN   # Close
                    else
                        COLOR=$C_WHITE  # Searching
                    fi
                fi
            fi
        fi
        
        printf "${C_BLUE}┃${C_RESET} #%-3d ${C_BLUE}┃${C_RESET} ${COLOR}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-4s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} %-4s ${C_BLUE}┃${C_RESET}\n" \
               "$i" "$ITER" "$RST" "$VIOL" "$MID" "$FOUND" "$TIME"
    done
    
    echo -e "${C_BLUE}┗━━━━━━┻━━━━━━━━━━┻━━━━━━━━━━┻━━━━━━┻━━━━━━━━━━┻━━━━━━━━━━┻━━━━━━┛${C_RESET}"
    
    # Footer Status
    if [ "$TOTAL_FOUND" -gt 0 ]; then
        echo -e "\n${C_GREEN}>>> SUCCESS: Found $TOTAL_FOUND sequences! (Check results folder) <<<${C_RESET}"
    else
        echo -e "\n${C_GRAY}Searching... (Press Ctrl+C to stop)${C_RESET}"
    fi

    # Refresh Rate (0.5s is smooth enough)
    sleep 0.5
done