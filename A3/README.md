# p3

## Compilation

    make            # builds all tools
    make diskinfo   # builds diskinfo only
    make disklist   # builds disklist only
    make diskget    # builds diskget only
    make diskput    # builds diskput only
    make clean

## Executables Produced 

    diskinfo
    disklist
    diskget
    diskput

## Overview of the implemented features 

### diskinfo
    Status: Fully implemented
    Description: Reads the superblock and FAT area of the disk image, printing filesystem metadata exactly in the required format.

### disklist
    Status: Fully implemented and tested
    Description: Lists directory contents inside the filesystem.

### diskget
    Status: Fully implemented and correct
    Description: Extracts a file from the disk image into the host filesystem.

### diskput
    Status: Fully implemented and validated
    Description: Places a host file into the CSC360FS filesystem.

## Testing Status
    All four programs were tested on linux.csc.uvic.ca against:
        Instructor-provided test images (4.1, 4.2, 4.3, 4.4)
        Fresh disk images created with mkfs
    Edge cases:
        empty files
        small files
        large files
        non-existent directory paths
        full FAT allocations
    Results: Everything behaves as intended and matches expected output.

