#!/usr/bin/env python3
"""
LLM API Key Rotator - Use multiple API keys without worrying about tokens
Rotates through 4 API keys for Gemini, Grok, Kimi
"""

import os
import time
import random
from typing import List, Dict, Any, Optional

class LLMKeyRotator:
    def __init__(self):
        self.keys = [
            {"provider": "gemini", "key": "AQ.Ab8RN6KQ8_gfqJVdLD1qVxo29H-6t2Xll53XmEa_4oWg0SMh3Q", "model": "gemini-pro"},
            {"provider": "gemini", "key": "AQ.Ab8RN6L5r2t0KtwU485HgLW1VVtNZYvV4QzAbUKoN9HfGgIgZw", "model": "gemini-pro"},
            {"provider": "grok", "key": "grok_04532454a5315216c7c6bcad6c269ab9_958750ef61bbf4d5aa2657a6108bcde1a4a015611ad688215e87ff50e013207e", "model": "grok-1"},
            {"provider": "kimi", "key": "sk-zuYTyzN2OdfpVJPFjOPZwD8TIIl5MWtgf27uW7trIhzOWrbC", "model": "moonshot-v1-8k"},
        ]
        self.current_index = 0
        self.usage_count = [0] * len(self.keys)
        self.last_used = [0] * len(self.keys)
        
    def get_key(self) -> Dict[str, Any]:
        """Get next available key with rotation"""
        # Rotate through keys, prefer least used
        min_usage = min(self.usage_count)
        candidates = [i for i, c in enumerate(self.usage_count) if c == min_usage]
        idx = random.choice(candidates)
        self.current_index = idx
        self.usage_count[idx] += 1
        self.last_used[idx] = time.time()
        return self.keys[idx]
    
    def get_all_keys(self) -> List[Dict[str, Any]]:
        return self.keys
    
    def get_stats(self) -> Dict[str, Any]:
        return {
            "total_keys": len(self.keys),
            "usage": self.usage_count,
            "providers": list(set(k["provider"] for k in self.keys))
        }

# Global rotator instance
rotator = LLMKeyRotator()

def get_llm_key():
    return rotator.get_key()

if __name__ == '__main__':
    r = LLMKeyRotator()
    for i in range(10):
        k = r.get_key()
        print(f"Request {i+1}: {k['provider']} - {k['model']} (usage tracked)")
    print(f"\nStats: {r.get_stats()}")
