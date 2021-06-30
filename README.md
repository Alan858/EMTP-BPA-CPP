# EMTP-BPA-CPP
C++ translation from EMTP-BPA Fortran using fable  
Licensed under the [MIT License](https://opensource.org/licenses/MIT)  
Copyright(c) 2021, [Dr. Alan W. Zhang](https://github.com/Alan858/EMTP-BPA-CPP)  

The source codes are compiled under MS Visual Studio 2019 community version.  
Set language standard to C++ 17.  

1. ConsoleEMTP-BPA.exe  

   It takes one input data file to run the program. If no input file was given,  
   the program will ask enter the input file name.  
   if the input data file is test.dat, then the program will put log infprmation  
   to test.dat.log and output results to test.dat.out in the same folder  
   
   The input data format is described in "EMTP Rule Book" in the Docs folder, or you can try ATPDraw  

   
2. case description

case0001 - single-phase series RLC circuit with step voltage input from "EMTP Primer.pdf"  
case0002 - single-phase parallel RLC circuit with capacitor discharge from "EMTP Primer.pdf"  
case0003 - RLC circuit from ATP email users using ATPDraw. The atp file using "/" cards  
case0004 - LINE CONSTANTS CALCULATIONS, "EMTP Primer.pdf" Section 3, Case 3, 500 kV  
case0005 - LINE CONSTANTS CALCULATIONS, "EMTP Primer.pdf" Section 3, Case 3, 345 kV  
case0006 - LINE CONSTANTS CALCULATIONS, "EMTP Primer.pdf" Section 3, Case 3, 220 kV  
case0007 - Capacitor Switching, "EMTP Primer.pdf" Section 5, Case 5: Capacitor Switch Recovery Voltages  
case0008 - Capacitor Switching, "EMTP Primer.pdf" Section 5, Case 5: Back-to-Back Capacitor  
           Banks With Current-Limiting Reactors
case0009 - Capacitor Switching, "EMTP Primer.pdf" Section 5, Case 5: Restrike Simulation  
case0010 - Steady-state solution with a fault, "EMTP Primer.pdf" Section 6, Case 6: Parallel EHV Line  
           Resonance
case0011 - Reclosing Of Transmission Lines, "EMTP Primer.pdf" Section 7, Case 7  
case0012 - Lightning surge studies, "EMTP Primer.pdf" Section 4, Case 4



