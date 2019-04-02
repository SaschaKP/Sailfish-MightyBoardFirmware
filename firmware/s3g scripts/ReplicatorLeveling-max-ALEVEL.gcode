(**** Replicator 1 & 2 leveling script ****)
(**** Do not run on Thing-o-Matics or Cupcakes: their homing is backwards ***)
M103 (Disable RPM)
M73 P0 (enable build progress)
G21 (set units to mm)
G90 (set positioning to absolute)
(**** Begin homing ****)
G162 Z F3000 ; Home Z maximum
G28 X Y ; (home XY axes maximum)
M132 X Y A B ; Recall home offsets for everything but Z
G1 X0 Y0 F2000 ; center of the platform after loading home offsets for x and y
G161 Z F800
G92 Z-5 ; Set Z to 5
G1 Z0
G161 Z F50 ; Home Z maximum slowly
M132 Z ; Recall home offsets for Z
(**** End homing ****)
(**** Disable the X & Y stepper motors so that the nozzle can be moved ****)
(**** about to any position over the platform allowing multi-point     ****)
(**** leveling and testing with a dial indicator or other test guage.  ****)
M18 X Y A B (Leave Z stepper enabled; disable the others)
M71 (By hand move the ex-truder to different positions over the  build platform....)
M71 (Adjust the spacing  between the extrudernozzle and platform with the knobs...)
M71 (under the platform  and a sheet of paperplaced between the  platform and the...)
M71 (nozzle. When you aredone, press the     center button.)
; M71 (Press the center    button when finished)
(**** All done -- disable all the stepper motors ****)
M18 X Y Z A B (Disable all stepper motors)
