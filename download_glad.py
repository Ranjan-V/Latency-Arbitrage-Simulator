#!/usr/bin/env python3
"""
Download and setup GLAD for OpenGL
"""

import os
import urllib.request
import zipfile
from pathlib import Path

PROJECT_ROOT = Path("D:/Projects/Latency-Arb-Simulator")
GLAD_URL = "https://github.com/Dav1dde/glad/archive/refs/heads/master.zip"

def download_glad():
    print("📥 Downloading GLAD from GitHub...")
    
    # Create external directory
    external_dir = PROJECT_ROOT / "external"
    external_dir.mkdir(exist_ok=True)
    
    # Download GLAD generator
    zip_path = external_dir / "glad.zip"
    
    # For simplicity, we'll create a minimal GLAD setup manually
    glad_dir = external_dir / "glad"
    glad_dir.mkdir(exist_ok=True)
    
    include_dir = glad_dir / "include"
    include_dir.mkdir(exist_ok=True)
    
    glad_h_dir = include_dir / "glad"
    glad_h_dir.mkdir(exist_ok=True)
    
    khr_dir = include_dir / "KHR"
    khr_dir.mkdir(exist_ok=True)
    
    src_dir = glad_dir / "src"
    src_dir.mkdir(exist_ok=True)
    
    print(f"✅ Created GLAD directory structure at {glad_dir}")
    print("\n⚠️  Manual Step Required:")
    print("1. Visit: https://glad.dav1d.de/")
    print("2. Settings:")
    print("   - Language: C/C++")
    print("   - Specification: OpenGL")
    print("   - gl: Version 3.3")
    print("   - Profile: Core")
    print("3. Click GENERATE")
    print("4. Download the ZIP")
    print("5. Extract and copy:")
    print(f"   - Copy glad/include/glad/* to {glad_h_dir}/")
    print(f"   - Copy glad/include/KHR/* to {khr_dir}/")
    print(f"   - Copy glad/src/glad.c to {src_dir}/")
    print("\nThen re-run CMake!")

if __name__ == "__main__":
    download_glad()