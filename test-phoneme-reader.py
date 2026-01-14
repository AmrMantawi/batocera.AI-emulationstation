#!/usr/bin/env python3
"""
Test script for phoneme queue - reads phonemes from shared memory and displays them
This mimics what EmulationStation does
"""

import mmap
import time
import struct
import sys

def main():
    shared_mem_path = "/tts_phoneme_queue"
    
    try:
        # Open shared memory (read-write so we can update read_index)
        with open(f"/dev/shm{shared_mem_path}", "r+b") as f:
            shared_mem = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_WRITE)
        
        print("Connected to phoneme queue. Waiting for phonemes...")
        print("(This is what EmulationStation does)")
        print()
        
        write_index_offset = 0
        read_index_offset = 4
        shutdown_offset = 8
        header_size = 16
        phoneme_data_size = 24  # int64(8) + float(4) + padding(4) + uint64(8)
        
        # Initialize from the current read_index in shared memory
        consumer_read_index = struct.unpack('I', shared_mem[read_index_offset:read_index_offset+4])[0]
        
        print(f"Starting from read_index: {consumer_read_index}")
        print()
        
        while True:
            try:
                # Read header atomics
                write_index = struct.unpack('I', shared_mem[write_index_offset:write_index_offset+4])[0]
                read_index = struct.unpack('I', shared_mem[read_index_offset:read_index_offset+4])[0]
                shutdown_flag = struct.unpack('?', shared_mem[shutdown_offset:shutdown_offset+1])[0]
                
                if shutdown_flag:
                    print("Shutdown signal received")
                    break
                
                # Process all new phonemes between our last position and the current write index
                while consumer_read_index != write_index:
                    # Read phoneme data (24 bytes per phoneme with padding)
                    offset = header_size + (consumer_read_index * phoneme_data_size)
                    phoneme_bytes = shared_mem[offset:offset + phoneme_data_size]
                    
                    if len(phoneme_bytes) >= phoneme_data_size:
                        try:
                            phoneme_id = struct.unpack('<q', phoneme_bytes[0:8])[0]
                            duration_seconds = struct.unpack('<f', phoneme_bytes[8:12])[0]
                            timestamp_us = struct.unpack('<Q', phoneme_bytes[16:24])[0]
                        except struct.error as e:
                            print(f"Unpack error at offset {offset}, bytes available: {len(phoneme_bytes)}, error: {e}")
                            break
                        
                        # Validate the data
                        if duration_seconds <= 0 or duration_seconds > 10.0:
                            print(f"Skipping phoneme {phoneme_id} with invalid duration {duration_seconds}s")
                            consumer_read_index = (consumer_read_index + 1) % 1024
                            shared_mem.seek(read_index_offset)
                            shared_mem.write(struct.pack('I', consumer_read_index))
                            shared_mem.flush()
                            continue
                        
                        # This is what EmulationStation displays:
                        print(f"Phoneme ID: {phoneme_id}, Duration: {duration_seconds:.3f}s")
                        
                        # Move to next phoneme
                        consumer_read_index = (consumer_read_index + 1) % 1024
                        
                        # Update the shared memory's read_index
                        shared_mem.seek(read_index_offset)
                        shared_mem.write(struct.pack('I', consumer_read_index))
                        shared_mem.flush()
                    else:
                        break
                
                time.sleep(0.001)  # Small delay to avoid busy waiting
                
            except KeyboardInterrupt:
                print("\nStopping...")
                break
            except Exception as e:
                import traceback
                print(f"Error: {e}")
                traceback.print_exc()
                time.sleep(0.1)
        
        shared_mem.close()
        
    except FileNotFoundError:
        print(f"Shared memory not found: {shared_mem_path}")
        print("Make sure TTSProcessor is running")
    except Exception as e:
        print(f"Failed to connect: {e}")

if __name__ == "__main__":
    main()

