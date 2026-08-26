# Deterministic Bash/Readline prompt for the wrapped-prompt resize reproducer.
# The visible prompt is 45 columns and includes Ghostty's OSC 133 annotations.
PS1='\[\e]133;P;k=i\a\]toppk@foundation:~/workspace/xterm-plus (0)$ \[\e]133;B\a\]'
PROMPT_COMMAND='printf "\e]133;A;redraw=last;cl=line\a"'
