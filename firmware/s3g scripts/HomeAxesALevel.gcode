M103
M73 P0
G21 (set units to mm)
G90 (set positioning to absolute)
G162 Z F3000 ; Home Z maximum
G28 X Y ; (home XY axes maximum)
M132 X Y A B ; Recall home offsets for everything but Z
G1 X0 Y0 F2000 ; center of the platform after loading home offsets for x and y
G161 Z F800
G92 Z-5 ; Set Z to 5
G1 Z0
G161 Z F50 ; Home Z maximum slowly
M132 Z ; Recall home offsets for Z
