# ATMT Racing Truck - Calibration and Setup Guide

## 🎯 Three-Stage Compass Solution

### **Stage 1: Initial Calibration Program**
Use for first-time setup and whenever you need to recalibrate:

```bash
# Upload calibration program
pio run -e calibration -t upload -t monitor

# Follow prompts to:
# 1. Calibrate compass (manually rotate truck)
# 2. Test all sensors  
# 3. Test actuators
# 4. Configure settings
# 5. Save everything to EEPROM
```

### **Stage 2: Racing Program**  
The main autonomous racing firmware:

```bash
# Upload racing program
pio run -e control_loop -t upload -t monitor

# Racing program will:
# 1. Load calibration from EEPROM (from Stage 1)
# 2. Start racing immediately with accurate compass  
# 3. Continue improving calibration during racing (background)
```

### **Stage 3: Background Auto-Calibration**
Automatic fallback if EEPROM data is corrupted or missing:

- Racing program detects missing calibration
- Enables background auto-calibration during driving
- Compass accuracy improves as truck turns during races
- Self-calibrates without any manual intervention

## 🔧 Usage Workflow

### **Initial Setup (one time):**
1. Upload calibration program: `pio run -e calibration -t upload`
2. Follow calibration menu to calibrate compass and test hardware
3. Calibration gets saved to EEPROM permanently

### **Racing (normal operation):**
1. Upload racing program: `pio run -e control_loop -t upload`  
2. Truck starts immediately with perfect compass accuracy
3. Background refinement continues during racing

### **Emergency/Corruption Recovery:**
- If EEPROM gets corrupted, racing program automatically falls back to background auto-calibration
- No manual intervention needed - just drive and compass improves automatically

## ✅ **Perfect for Racing!**
- **Setup once**: Use calibration program for initial setup
- **Race anytime**: Load racing program and go immediately  
- **Zero maintenance**: Background calibration handles everything automatically
- **Bulletproof**: Automatic fallback if anything goes wrong

## 📁 **Files**
- `z_calibration_setup.cpp` - Dedicated calibration program
- `z_main_control_loop.cpp` - Main racing program  
- `platformio.ini` - Build environments for both programs