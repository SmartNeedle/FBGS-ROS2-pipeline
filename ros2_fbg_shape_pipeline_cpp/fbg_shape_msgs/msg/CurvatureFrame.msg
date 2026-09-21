std_msgs/Header header
uint64 line_number
float64 source_timestamp
# Deliberate project convention: positions in mm; all curvature components in 1/mm.
float64[] arc_lengths
float64[] curvature
# Radians, after orientation sign/offset calibration.
float64[] angle
float64[] kappa_x
float64[] kappa_y
float64[] kappa_z
float64[] temperature
