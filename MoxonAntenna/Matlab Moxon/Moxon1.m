set(groot, 'DefaultFigureWindowStyle', 'docked');
% MOXON PARAMETERS
width = 0.003;
% A = 0.230;
% B = 0.033;
% C = 0.007;
% D = 0.046;

A = 0.240;
B = 0.035;
C = 0.010;
D = 0.047;
E = B + C + D;
half_A = A/2;
half_E = E/2;
gap_width = width * 5;

% A = 0.219;
% B = 0.033;
% C = 0.005;
% D = 0.046;

fc = 433e6;

%%Create pcbStack object
Moxon = pcbStack;


%Board
BoardW = antenna.Rectangle;
BoardW.Center = [(A/2) E/2];
BoardW.Length = A;
BoardW.Width = E;
BoardH1 = antenna.Rectangle;
BoardH1.Center = [gap_width+(((A-(gap_width*3))/2)/2) E/2];
BoardH1.Length = (A-((gap_width)*3))/2;
BoardH1.Width = E-((gap_width)*2);
BoardH2 = antenna.Rectangle;
BoardH2.Center = [(gap_width+(((A-(gap_width*3))/2)/2))+(((A-(gap_width*3))/2)+gap_width) E/2];
BoardH2.Length = (A-(gap_width*3))/2;
BoardH2.Width = E-(gap_width*2);
Board = BoardW - BoardH1 - BoardH2;
Moxon.BoardShape = Board;

Moxon.BoardThickness = 0.00146;
DielectricLayer = dielectric("Name",'FR4',"EpsilonR",4.5,"LossTangent",0.026,"Thickness",0.00146);


%Reflector
ReflectorL = antenna.Rectangle;
ReflectorL.Center = [width/2 D/2];
ReflectorL.Length = width;
ReflectorL.Width = D;
ReflectorC = antenna.Rectangle;
ReflectorC.Center = [A/2 width/2];
ReflectorC.Length = A;
ReflectorC.Width = width;
ReflectorR = antenna.Rectangle;
ReflectorR.Center = [A-width/2 D/2];
ReflectorR.Length = width;
ReflectorR.Width = D;
Reflector = ReflectorL + ReflectorC + ReflectorR;

%Driven Element
DrivenL1 = antenna.Rectangle;
DrivenL1.Center = [width/2 E-B/2];
DrivenL1.Length = width;
DrivenL1.Width = B;
DrivenR1 = antenna.Rectangle;
DrivenR1.Center = [A-width/2 E-B/2];
DrivenR1.Length = width;
DrivenR1.Width = B;
DrivenC = antenna.Rectangle;
DrivenC.Center = [A/2 E-width/2];
DrivenC.Length = A;
DrivenC.Width = width;
Driven = DrivenL1 + DrivenR1 + DrivenC;

TraceLayer = Reflector + Driven;
Moxon.Layers = {TraceLayer, DielectricLayer};
FeedDiam = gap_width;

Moxon.FeedLocations = [A/2 E-width/2 1];
Moxon.FeedDiameter = 0.002;
Moxon.FeedVoltage = 3.3;
% load = lumpedElement(Impedance=complex(110, -3.68));
% Moxon.Load = load;


fig1 = figure();
Moxon.show()
fig2 = figure();
Moxon.pattern(fc)
% 
% 
fig3 = figure();
Moxon.patternAzimuth(fc)

freqrange = 400e6:1e6:525e6;
fig5 = figure(Name="Imp");
resonantFrequency(Moxon,freqrange)
% impedance(Moxon,freqrange);
fig4 = figure(Name="Sparam");
resonantFrequency(Moxon,freqrange,Method="Sparameters")


