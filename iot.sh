#!/bin/bash

ACTION="start"  # default

# Parse options
while getopts ":rp" opt; do
  case ${opt} in
    r )
      ACTION="restart"
      ;;
    p )
      ACTION="stop"
      ;;
    \? )
      echo "❌ Invalid option: -$OPTARG" >&2
      echo "⚙️ Usage: ./iot-dashboard.sh [-r (restart) | -p (stop)]"
      exit 1
      ;;
  esac
done

function start_services {
  echo "🚀 Starting IoT Dashboard Services..."
  docker compose up -d > /dev/null
  sleep 3

  echo "🧠 Starting Node.js MQTT → InfluxDB bridge..."
  pm2 restart influx-bridge > /dev/null 2>&1

  if ! pm2 list | grep -q "influx-bridge.*online"; then
    pm2 start influx-bridge.js --name influx-bridge > /dev/null
  fi

  pm2 save > /dev/null
  echo "✅ Services started."
}

function stop_services {
  echo "🛑 Stopping IoT Dashboard Services..."
  docker compose down --volumes --remove-orphans > /dev/null 2>&1
  pm2 stop influx-bridge > /dev/null 2>&1 || true
  echo "✅ Services stopped."
}

function restart_services {
  stop_services
  start_services
}

# Run the selected action
case "$ACTION" in
  start)
    start_services
    ;;
  stop)
    stop_services
    ;;
  restart)
    restart_services
    ;;
esac

# Always show service status
echo ""
echo "📊 Docker Services:"
docker ps --filter "name=influxdb" --filter "name=grafana" --filter "name=mqtt"

echo ""
echo "🧠 PM2 Processes:"
pm2 ls

# Only show URLs for start & restart
if [[ "$ACTION" != "stop" ]]; then
  echo ""
  echo "🌐 Service URLs:"
  echo "🔗 Grafana:   http://localhost:3000"
  echo "🔗 InfluxDB:  http://localhost:8086"
  echo "🔗 MQTT:      mqtt://localhost:1883"
fi
