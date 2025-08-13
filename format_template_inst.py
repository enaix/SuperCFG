#!/usr/bin/env python3
import re
import sys

def format_template_error(text):
    """Format C++ template error messages with proper indentation."""
    
    # Replace common template delimiters with newlines and track nesting
    lines = []
    current_line = ""
    indent_level = 0
    
    i = 0
    while i < len(text):
        char = text[i]
        
        if char == '<':
            # Check if this is a template parameter list
            if i > 0 and (text[i-1].isalnum() or text[i-1] in '_>'):
                current_line += char
                lines.append("  " * indent_level + current_line)
                current_line = ""
                indent_level += 1
            else:
                current_line += char
        elif char == '>':
            if current_line.strip():
                lines.append("  " * indent_level + current_line)
                current_line = ""
            indent_level = max(0, indent_level - 1)
            lines.append("  " * indent_level + ">")
        elif char == ',':
            # Add comma and start new line at same indent level
            current_line += char
            lines.append("  " * indent_level + current_line)
            current_line = ""
        elif char == '\n':
            if current_line.strip():
                lines.append("  " * indent_level + current_line)
                current_line = ""
            lines.append("")  # Preserve original line breaks
        else:
            current_line += char
        
        i += 1
    
    if current_line.strip():
        lines.append("  " * indent_level + current_line)
    
    return '\n'.join(lines)

def clean_template_names(text):
    """Clean up common verbose template patterns."""
    
    # Replace std::integral_constant with IC for brevity
    text = re.sub(r'std::integral_constant<([^,]+),\s*(\d+)>', r'IC<\1, \2>', text)
    
    # Simplify ConstStr patterns
    text = re.sub(r'ConstStr<ConstStrContainer<(\d+)>\{"([^"]+)"\}>', r'Str<"\2">', text)
    
    # Simplify std::pair
    text = re.sub(r'std::pair<([^,]+),\s*([^>]+)>', r'pair<\1, \2>', text)
    
    # Simplify std::tuple
    text = re.sub(r'std::tuple<', r'tuple<', text)
    
    return text

def main():
    if len(sys.argv) > 1:
        with open(sys.argv[1], 'r') as f:
            content = f.read()
    else:
        content = sys.stdin.read()
    
    # Clean up verbose patterns first
    cleaned = clean_template_names(content)
    
    # Format with indentation
    formatted = format_template_error(cleaned)
    
    print(formatted)

if __name__ == "__main__":
    main()
