#!/usr/bin/env fish

echo "Starting Sand3 Workshop Backend & Frontend Development Servers..."

# Change to script directory's parent (site/)
set SCRIPT_DIR (status dirname)
cd "$SCRIPT_DIR/.."

# 1. Start backend server
echo "Starting Backend API on http://localhost:3000..."
cd server
npx tsx watch src/index.ts &
set SERVER_PID $last_pid
cd ..

# 2. Start frontend dev server
echo "Starting Angular Frontend on http://localhost:4200..."
npx ng serve --port 4200 &
set ANGULAR_PID $last_pid

function cleanup --on-signal SIGINT --on-signal SIGTERM
    echo ""
    echo "Shutting down development servers..."
    kill $SERVER_PID 2>/dev/null
    kill $ANGULAR_PID 2>/dev/null
    exit 0
end

echo "Both servers running! Press Ctrl+C to stop."
wait
