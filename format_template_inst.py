#!/usr/bin/env python3
import re
import sys

def count_template_params(text, start_pos):
    """Count the number of template parameters starting from a '<' character.
    
    Args:
        text: The full text
        start_pos: Position of the opening '<'
    
    Returns:
        Number of parameters in this template (0 for empty templates)
    """
    depth = 0
    param_count = 0
    i = start_pos
    
    while i < len(text):
        char = text[i]
        
        if char == '<':
            if depth == 0:
                param_count = 1  # At least one parameter
            depth += 1
        elif char == '>':
            depth -= 1
            if depth == 0:
                # Check if template was empty
                segment = text[start_pos:i+1]
                if segment.strip() == '<>':
                    return 0
                return param_count
        elif char == ',' and depth == 1:
            param_count += 1
        
        i += 1
    
    return param_count

def format_template_error(text, min_params=5):
    """Format C++ template error messages with proper indentation.
    
    Each template instantiation is evaluated independently. Only templates
    with >= min_params parameters are formatted with indentation.
    
    Args:
        text: The text to format
        min_params: Minimum number of template parameters required to trigger formatting.
    """
    
    lines = []
    current_line = ""
    indent_level = 0
    indent_stack = []  # Track which levels should be indented
    
    i = 0
    while i < len(text):
        char = text[i]
        
        if char == '<':
            # Check if this is a template parameter list
            if i > 0 and (text[i-1].isalnum() or text[i-1] in '_>'):
                # Count parameters for this template
                param_count = count_template_params(text, i)
                should_indent = param_count >= min_params
                
                current_line += char
                
                if should_indent:
                    # Apply indentation for this template
                    lines.append("  " * indent_level + current_line)
                    current_line = ""
                    indent_level += 1
                    indent_stack.append(True)
                else:
                    # Don't indent this template
                    indent_stack.append(False)
            else:
                current_line += char
        elif char == '>':
            if indent_stack and indent_stack[-1]:
                # This template was indented
                if current_line.strip():
                    lines.append("  " * indent_level + current_line)
                    current_line = ""
                indent_level = max(0, indent_level - 1)
                lines.append("  " * indent_level + ">")
                indent_stack.pop()
            else:
                # This template was not indented
                current_line += char
                if indent_stack:
                    indent_stack.pop()
        elif char == ',':
            if indent_stack and indent_stack[-1]:
                # We're in an indented template
                current_line += char
                lines.append("  " * indent_level + current_line)
                current_line = ""
            else:
                # We're in a non-indented template
                current_line += char
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
    text = re.sub(r'std::integral_constant<([^,]+),\s*(\d+)>', r'IC<\2>', text)
    
    # Simplify ConstStr patterns
    text = re.sub(r'ConstStr<ConstStrContainer<(\d+)>\{"([^"]+)"\}>', r'Str<"\2">', text)
    
    # Simplify std::pair
    text = re.sub(r'std::pair<([^,]+),\s*([^>]+)>', r'pair<\1, \2>', text)
    
    # Simplify std::tuple
    text = re.sub(r'std::tuple<', r'tuple<', text)
    
    # Replace size_t max
    text = re.sub(r'18446744073709551615', r'SIZE_T_MAX', text)
    return text

def main():
    # Default minimum parameter threshold
    min_params = 5
    
    # Parse command line arguments
    args = sys.argv[1:]
    input_file = None
    
    i = 0
    while i < len(args):
        if args[i] in ['-k', '--min-params']:
            if i + 1 < len(args):
                min_params = int(args[i + 1])
                i += 2
            else:
                print("Error: -k/--min-params requires a value", file=sys.stderr)
                sys.exit(1)
        else:
            input_file = args[i]
            i += 1
    
    # Read input
    if input_file:
        with open(input_file, 'r') as f:
            content = f.read()
    else:
        content = sys.stdin.read()
    
    # Clean up verbose patterns first
    cleaned = clean_template_names(content)
    
    # Format with indentation (each template evaluated independently)
    formatted = format_template_error(cleaned, min_params=min_params)
    
    print(formatted)

if __name__ == "__main__":
    main()

