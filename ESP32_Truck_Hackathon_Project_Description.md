# ESP32 Truck Hackathon Project Description
## ScaniaHack 2026 - Autonomous Racing System

### Project Overview

The **Autonomous Toy Monster Truck (ATMT)** this is a for ScaniaHack 2026, focusing on developing a cooperative, learning-based autonomous racing system using two ESP32-powered toy trucks.

### 🎯 Project Goals

**Primary Objective**: Build a cooperative autonomous racing system where trucks:
- Autonomously explore and map a bounded track
- Optimize racing strategies using collected data  
- Communicate via backend server for cooperative learning
- Demonstrate real-time telemetry and control

**Technical Challenge**: Transform simple toy trucks into intelligent racing vehicles capable of learning optimal racing lines through exploration and data analysis.

---

## 🏗️ System Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        TRUCK FIRMWARE (ESP32)                   │
│                                                                 │
│ ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│ │   Sensors   │  │  Control    │  │ Telemetry   │              │
│ │             │  │ State       │  │             │              │
│ │ • 4x US     │  │ Machine     │  │ • MQTT      │              │
│ │ • IMU       │  │             │  │ • JSON      │              │
│ │ • Compass   │  │ IDLE        │  │ • 1Hz       │              │
│ │             │  │ EXPLORE     │  │             │              │
│ │ Kalman      │  │ RECOVER     │  │             │              │
│ │ Filtering   │  │ STOP        │  │             │              │
│ └─────────────┘  └─────────────┘  └─────────────┘              │
│                                                                 │
└─────────────────────┼───────────────────────────────────────────┘
                      │ WiFi/MQTT
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                    BACKEND INFRASTRUCTURE                       │
│                                                                 │
│ ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│ │ MQTT Broker │  │ Database    │  │ Dashboard   │              │
│ │ (Pi5)       │  │ (SQLite)    │  │ (Flask)     │              │
│ │             │  │             │  │             │              │
│ │ • Message   │  │ • Telemetry │  │ • Real-time │              │
│ │   Routing   │  │ • Track     │  │   Monitoring│              │
│ │ • QoS       │  │   Data      │  │ • WebSocket │              │
│ └─────────────┘  └─────────────┘  └─────────────┘              │
│                                                                 │
│ ┌─────────────┐  ┌─────────────┐                               │ 
│ │ AI Training │  │ Race        │                               │
│ │ (Future)    │  │ Optimizer   │                               │
│ │             │  │ (Future)    │                               │
│ │ • Track     │  │             │                               │
│ │   Learning  │  │ • Line      │                               │
│ │ • Behavior  │  │   Optim.    │                               │
│ │   Analysis  │  │ • Speed     │                               │
│ └─────────────┘  └─────────────┘                               │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🚧 Development Phases

### Phase 1: Track Exploration (Current Focus)
**Goal**: Autonomous track exploration with clean data collection

**Key Features**:
- Wall-following navigation for consistent lap completion
- Conservative speed prioritizing data quality over performance  
- Comprehensive sensor fusion for robust perception
- Real-time telemetry streaming for backend analysis

### Phase 2: Track Learning (Planned)
**Goal**: Extract track geometry and patterns from exploration data

**Key Features**:
- Data processing to identify track segments and characteristics
- Pattern recognition for turns, straightaways, and optimal racing lines
- Digital track map generation from telemetry history
- Performance baseline establishment

### Phase 3: Racing Optimization (Planned)
**Goal**: Intelligent racing with optimized strategies

**Key Features**:
- Racing line calculation for optimal cornering
- Continuous learning from racing performance data

---

## 🏆 ScaniaHack 2026 Context

### Innovation Highlights
- **Real Autonomous Vehicles**: Working hardware demonstration, not simulation
- **Scalable Architecture**: Foundation applicable to larger autonomous systems
- **Learning Capabilities**: Continuous improvement through data-driven optimization
- **Educational Value**: Open-source platform for autonomous systems learning

### Demonstration Capabilities
1. **Live Autonomous Racing**: Real-time racing demonstration with multiple vehicles
2. **Data Visualization**: Live telemetry dashboard showing decision-making process
3. **Learning Progress**: Demonstrable improvement in lap times and racing lines
4. **Cooperative Behavior**: Multi-vehicle coordination and strategy sharing

### Technical Challenges Addressed
- **Sensor Fusion**: Robust perception from multiple sensor modalities
- **Real-time Control**: Low-latency autonomous navigation and decision-making
- **Distributed Systems**: Networked vehicles sharing data and strategies
- **Online Learning**: Adaptation and improvement from real-world performance data

---

## 📊 Success Metrics

### Phase 1 (Exploration) - Current Target
- **Autonomous Laps**: Complete 10+ consecutive laps without intervention
- **Data Quality**: Consistent, reliable sensor data with minimal noise
- **System Reliability**: Robust operation with graceful error handling
- **Real-time Monitoring**: Live dashboard showing truck status and decision-making

### Phase 2 (Learning) - Development Target  
- **Track Mapping**: Accurate digital representation of track geometry
- **Pattern Recognition**: Automatic identification of racing line opportunities
- **Performance Analysis**: Baseline lap times and efficiency metrics

### Phase 3 (Racing) - Development Target
- **Optimized Racing**: Demonstrable improvement over exploration baseline
- **Multi-truck Racing**: Coordinated racing strategies between vehicles
- **Adaptive Behavior**: Real-time strategy adjustment based on conditions

---

## 🤝 Project Vision & Impact

### Educational Mission
This project serves as a comprehensive learning platform covering:
- **Embedded Systems**: Real-time control and sensor integration
- **Autonomous Navigation**: Perception, planning, and control algorithms  
- **Distributed Systems**: Multi-agent coordination and communication
- **Machine Learning**: Online learning and performance optimization

### Open Source Philosophy
- **Modular Design**: Accessible components for learning and modification
- **Comprehensive Documentation**: Detailed guides for all system aspects
- **Community Contribution**: Multiple entry points for different skill levels
- **Real-world Application**: Physical testing with immediate feedback

---

## 📚 Related Documentation

For detailed technical implementation, setup instructions, and development guides, see:

- **[Main Project README](readme.md)** - Complete project overview and quick start
- **[Firmware Documentation](EmbeddedSw/README.md)** - Detailed technical implementation
- **[Hardware Guide](toy-truck/electronics/hardware.md)** - Component specifications and assembly
- **[Dashboard Setup](truck_telemetry_dashboard/README.md)** - Real-time monitoring system
- **[OTA Process](arduino-ota-process.md)** - Wireless update procedures
- **[Development Setup](getstarted.md)** - Environment configuration and tools

---

**This project represents an innovative intersection of embedded systems, autonomous navigation, machine learning, and competitive programming - creating a comprehensive platform for hands-on learning and innovation in autonomous vehicle technology.**

*ScaniaHack 2026 - March 2026*