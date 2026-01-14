#!/usr/bin/env python3
"""
Simple test client for BMO AI Graphics
Connects to EmulationStation's socket server and sends test messages
"""
import socket
import os
import sys
import time
import json

SOCKET_PATH = "/tmp/local-llm.sock"

def send_message(sock, msg_dict):
    """Send a JSON message to the socket"""
    msg = json.dumps(msg_dict) + "\n"
    sock.send(msg.encode('utf-8'))
    print(f"Sent: {msg.strip()}")

def main():
    print(f"Connecting to EmulationStation at {SOCKET_PATH}...")
    
    # Wait for socket to be available
    for i in range(10):
        if os.path.exists(SOCKET_PATH):
            break
        print(f"Waiting for socket... ({i+1}/10)")
        time.sleep(1)
    else:
        print("Socket not found. Is EmulationStation running?")
        return
    
    try:
        # Connect to EmulationStation's server
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(SOCKET_PATH)
        print("Connected!")
        print()
        
        # Test 1: Send a simple message
        print("Test 1: Sending a message...")
        send_message(sock, {"message": "Hello from test client!"})
        time.sleep(1)
        
        # Test 2: Send individual phonemes (incremental)
        print("\nTest 2: Sending incremental phonemes...")
        phonemes = [
            ("HH", 100),
            ("AH", 120),
            ("L", 80),
            ("OW", 150),
        ]
        for phoneme, duration in phonemes:
            send_message(sock, {"phoneme": phoneme, "durationMs": duration})
            time.sleep(0.05)  # Small delay between phonemes
        
        time.sleep(2)
        
        # Test 3: Send batch phonemes
        print("\nTest 3: Sending batch phonemes...")
        send_message(sock, {
            "phonemes": "W ER L D",
            "phonemeDurations": [100, 120, 80, 140]
        })
        
        time.sleep(2)
        
        # Test 4: Send message with phonemes
        print("\nTest 4: Sending message with phonemes...")
        send_message(sock, {
            "message": "Testing speech synthesis!",
            "phonemes": "T EH S T IH NG",
            "phonemeDurations": [80, 100, 90, 80, 100, 120]
        })
        
        time.sleep(3)
        
        # Test 5: Interactive mode
        print("\n" + "="*50)
        print("Interactive mode - enter JSON or press Ctrl+C to quit")
        print("Examples:")
        print('  {"message": "Hello"}')
        print('  {"phoneme": "AA", "durationMs": 100}')
        print('  {"phonemes": "HH EH L OW", "phonemeDurations": [80,100,90,120]}')
        print("="*50)
        print()
        
        while True:
            try:
                line = input("> ")
                if not line.strip():
                    continue
                
                # Try to parse as JSON
                try:
                    msg_dict = json.loads(line)
                    send_message(sock, msg_dict)
                except json.JSONDecodeError:
                    # If not JSON, treat as a message
                    send_message(sock, {"message": line})
                    
            except EOFError:
                break
        
    except KeyboardInterrupt:
        print("\n\nShutting down...")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()
        print("Disconnected.")

if __name__ == "__main__":
    main()