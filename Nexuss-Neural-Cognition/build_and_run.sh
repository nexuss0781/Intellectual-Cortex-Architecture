#!/bin/bash

# ------------------------------------------------------------------------------
# Project Genesis: Phase III Build & Automation Script
# ------------------------------------------------------------------------------
# Purpose:
# 1. Builds the project (Core + 3 Test Suites + Main Sim).
# 2. Runs Physics Validation (Phase I).
# 3. Runs Sensory Validation (Phase II).
# 4. Runs Memory Validation (Phase III).
# 5. Executes the Pavlovian Learning Experiment.
# ------------------------------------------------------------------------------

# Exit immediately if any command fails
set -e

# Visual formatting
BOLD='\033[1m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BOLD}[Genesis] Initializing Build Pipeline...${NC}"

# 1. Directory Setup
if [ ! -d "build" ]; then
    echo -e "  > Creating build directory..."
    mkdir build
else
    echo -e "  > Build directory exists. Cleaning..."
    rm -rf build/*
fi

cd build

# 2. CMake Configuration
echo -e "${BOLD}[Genesis] Configuring Build System...${NC}"
cmake -DCMAKE_BUILD_TYPE=Release ..

# 3. Compilation
echo -e "${BOLD}[Genesis] Compiling Bio-Digital Substrate...${NC}"
if cmake --build . --config Release -- -j$(nproc); then
    echo -e "${GREEN}  > Compilation Successful.${NC}"
else
    echo -e "${RED}  > Compilation Failed.${NC}"
    exit 1
fi

# 4. Validation Suites
echo -e "${BOLD}[Genesis] Executing Validation Suites...${NC}"

echo -e "  > [1/3] Physics Integrity (Phase I)..."
if ./physics_tests; then
    echo -e "${GREEN}    [PASS] Physics Verified.${NC}"
else
    echo -e "${RED}    [FAIL] Physics Physics Failed.${NC}"
    exit 1
fi

echo -e "  > [2/3] Sensory Dynamics (Phase II)..."
if ./sensory_tests; then
    echo -e "${GREEN}    [PASS] Sensory Verified.${NC}"
else
    echo -e "${RED}    [FAIL] Sensory Logic Failed.${NC}"
    exit 1
fi

echo -e "  > [3/3] Memory & Plasticity (Phase III)..."
if ./memory_tests; then
    echo -e "${GREEN}    [PASS] Memory Architecture Verified.${NC}"
else
    echo -e "${RED}    [FAIL] Memory Logic Failed.${NC}"
    exit 1
fi

# 5. Main Simulation
echo -e "${BOLD}[Genesis] Launching Phase III Experiment: The Pavlovian Android...${NC}"
echo "----------------------------------------------------------------"
./genesis_sim
echo "----------------------------------------------------------------"

echo -e "${GREEN}[Genesis] Workflow Completed Successfully.${NC}"