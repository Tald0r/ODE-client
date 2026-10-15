#pragma once

// The step of an alignment value: 0 evil+ (up to -7501), 1 evil (to
// -2501), 2 neutral (to 2500), 3 good (to 7500), 4 good+ (to 10000), and 0
// past that. A value above 10000 is a negative one sent as 16 bits.
int ConvertAlignment(int alignment);
