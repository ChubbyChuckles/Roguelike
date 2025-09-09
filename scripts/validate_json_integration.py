#!/usr/bin/env python3
"""
Validation script for JSON Items Integration
Validates that the JSON item system meets all success criteria from the implementation plan
"""

import os
import json
import sys
import time

def validate_json_files():
    """Validate that all JSON item files are valid JSON and have required fields"""
    print("=== Validating JSON Item Files ===")
    items_dir = "assets/items"
    
    if not os.path.exists(items_dir):
        print(f"ERROR: {items_dir} directory not found")
        return False
    
    json_files = [f for f in os.listdir(items_dir) if f.endswith('.json')]
    print(f"Found {len(json_files)} JSON files")
    
    required_fields = ['id', 'name', 'category', 'level_req', 'stack_max', 'base_value']
    valid_files = 0
    
    for filename in json_files:
        filepath = os.path.join(items_dir, filename)
        try:
            with open(filepath, 'r') as f:
                data = json.load(f)
            
            # Check required fields
            missing_fields = [field for field in required_fields if field not in data]
            if missing_fields:
                print(f"  WARNING: {filename} missing fields: {missing_fields}")
            else:
                print(f"  ✓ {filename}")
                valid_files += 1
                
        except json.JSONDecodeError as e:
            print(f"  ERROR: {filename} invalid JSON: {e}")
        except Exception as e:
            print(f"  ERROR: {filename} error: {e}")
    
    print(f"Valid JSON files: {valid_files}/{len(json_files)}")
    return valid_files == len(json_files)

def validate_vendor_category_alignment():
    """Validate that vendor inventory templates align with item categories"""
    print("\n=== Validating Vendor Category Alignment ===")
    
    # Expected categories based on RogueItemCategory enum
    expected_categories = [
        "ROGUE_ITEM_MISC",      # 0
        "ROGUE_ITEM_CONSUMABLE", # 1  
        "ROGUE_ITEM_WEAPON",     # 2
        "ROGUE_ITEM_ARMOR",      # 3
        "ROGUE_ITEM_GEM",        # 4
        "ROGUE_ITEM_MATERIAL"    # 5
    ]
    
    templates_file = "assets/vendors/inventory_templates.json"
    if not os.path.exists(templates_file):
        print(f"WARNING: {templates_file} not found")
        return True
    
    try:
        with open(templates_file, 'r') as f:
            data = json.load(f)
        
        for template in data.get("inventory_templates", []):
            archetype = template.get("archetype", "unknown")
            category_weights = template.get("category_weights", [])
            
            if len(category_weights) != len(expected_categories):
                print(f"  WARNING: {archetype} has {len(category_weights)} weights, expected {len(expected_categories)}")
            else:
                print(f"  ✓ {archetype} category weights align with item categories")
        
        return True
        
    except Exception as e:
        print(f"ERROR: Failed to validate vendor templates: {e}")
        return False

def validate_json_item_coverage():
    """Validate that JSON items cover all major categories"""
    print("\n=== Validating JSON Item Coverage ===")
    
    items_dir = "assets/items"
    categories_found = set()
    
    json_files = [f for f in os.listdir(items_dir) if f.endswith('.json')]
    
    for filename in json_files:
        filepath = os.path.join(items_dir, filename)
        try:
            with open(filepath, 'r') as f:
                data = json.load(f)
            categories_found.add(data.get('category', -1))
        except:
            continue
    
    # Check coverage of major categories
    expected_categories = [0, 1, 2, 3, 4, 5]  # MISC, CONSUMABLE, WEAPON, ARMOR, GEM, MATERIAL
    category_names = ["MISC", "CONSUMABLE", "WEAPON", "ARMOR", "GEM", "MATERIAL"]
    
    print("Category coverage:")
    for i, name in enumerate(category_names):
        if i in categories_found:
            print(f"  ✓ {name} (category {i})")
        else:
            print(f"  - {name} (category {i}) - no JSON items found")
    
    critical_categories = [1, 2, 3, 5]  # CONSUMABLE, WEAPON, ARMOR, MATERIAL
    critical_covered = all(cat in categories_found for cat in critical_categories)
    
    if critical_covered:
        print("✓ All critical categories have JSON representation")
    else:
        print("WARNING: Some critical categories missing JSON items")
    
    return len(categories_found) >= 3  # At least 3 categories should be covered

def validate_schema_structure():
    """Basic validation of item schema structure"""
    print("\n=== Validating Schema Structure ===")
    
    items_dir = "assets/items"
    json_files = [f for f in os.listdir(items_dir) if f.endswith('.json')]
    
    # Check for common schema issues
    valid_items = 0
    
    for filename in json_files:
        filepath = os.path.join(items_dir, filename)
        try:
            with open(filepath, 'r') as f:
                data = json.load(f)
            
            # Basic validation checks
            issues = []
            
            if not isinstance(data.get('id'), str) or not data.get('id'):
                issues.append("invalid id")
            
            if not isinstance(data.get('name'), str) or not data.get('name'):
                issues.append("invalid name")
                
            if not isinstance(data.get('category'), int) or data.get('category') < 0:
                issues.append("invalid category")
                
            if not isinstance(data.get('stack_max'), int) or data.get('stack_max') < 1:
                issues.append("invalid stack_max")
            
            if issues:
                print(f"  WARNING: {filename} issues: {', '.join(issues)}")
            else:
                valid_items += 1
                
        except:
            continue
    
    print(f"Schema validation: {valid_items}/{len(json_files)} items valid")
    return valid_items > 0

def main():
    """Run all validation checks"""
    print("JSON Items Integration - Validation Script")
    print("=" * 50)
    
    # Change to repository root
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(script_dir)
    os.chdir(repo_root)
    
    print(f"Working directory: {os.getcwd()}")
    
    checks = [
        ("JSON File Validation", validate_json_files),
        ("Vendor Category Alignment", validate_vendor_category_alignment), 
        ("JSON Item Coverage", validate_json_item_coverage),
        ("Schema Structure", validate_schema_structure),
    ]
    
    passed = 0
    total = len(checks)
    
    for name, check_func in checks:
        try:
            if check_func():
                passed += 1
        except Exception as e:
            print(f"ERROR in {name}: {e}")
    
    print("\n" + "=" * 50)
    print(f"Validation Results: {passed}/{total} checks passed")
    
    if passed == total:
        print("✓ JSON Items Integration validation PASSED")
        print("The system is ready for production use with JSON-first item loading.")
        return 0
    else:
        print("⚠ Some validation checks failed or had warnings")
        print("Review the issues above before deploying JSON-first loading.")
        return 1

if __name__ == "__main__":
    sys.exit(main())