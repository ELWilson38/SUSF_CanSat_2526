%%Create Variables

%%Create pcbStack object
pcbobj = pcbStack;

%%Create board shape
    %Creating BoardShape metal layer.
        %Creating Rectangle1 shape.
        Rectangle1 = antenna.Rectangle;
        Rectangle1.Name = "Rectangle1";
        Rectangle1.Center = [0.116696 0.0451665];
        Rectangle1.Length = 0.24955;
        Rectangle1.Width = 0.10652;
        Rectangle1 = rotate(Rectangle1,0,[Rectangle1.Center,-1],[Rectangle1.Center,1]);
    BoardShape = Rectangle1;
pcbobj.BoardShape = BoardShape;

%%Create Stackup
    %Creating MetalLayer1 metal layer.
        %Creating Rectangle2 shape.
        Rectangle2 = antenna.Rectangle;
        Rectangle2.Name = "Rectangle2";
        Rectangle2.Center = [0.003 0.025];
        Rectangle2.Length = 0.006;
        Rectangle2.Width = 0.05;
        Rectangle2 = rotate(Rectangle2,0,[Rectangle2.Center,-1],[Rectangle2.Center,1]);
            %Creating Rectangle4 shape.
            Rectangle4 = antenna.Rectangle;
            Rectangle4.Name = "Rectangle4";
            Rectangle4.Center = [0.1175 0.003];
            Rectangle4.Length = 0.223;
            Rectangle4.Width = 0.006;
            Rectangle4 = rotate(Rectangle4,0,[Rectangle4.Center,-1],[Rectangle4.Center,1]);
        Rectangle2 = Rectangle2 + Rectangle4;%Add
            %Creating Rectangle3_Copy2 shape.
            Rectangle3_Copy2 = antenna.Rectangle;
            Rectangle3_Copy2.Name = "Rectangle3_Copy2";
            Rectangle3_Copy2.Center = [0.232 0.025];
            Rectangle3_Copy2.Length = 0.006;
            Rectangle3_Copy2.Width = 0.05;
            Rectangle3_Copy2 = rotate(Rectangle3_Copy2,0,[Rectangle3_Copy2.Center,-1],[Rectangle3_Copy2.Center,1]);
        Rectangle2 = Rectangle2 + Rectangle3_Copy2;%Add
        %Creating Rectangle7 shape.
        Rectangle7 = antenna.Rectangle;
        Rectangle7.Name = "Rectangle7";
        Rectangle7.Center = [0.06025 0.089];
        Rectangle7.Length = 0.1085;
        Rectangle7.Width = 0.006;
        Rectangle7 = rotate(Rectangle7,0,[Rectangle7.Center,-1],[Rectangle7.Center,1]);
            %Creating Rectangle3 shape.
            Rectangle3 = antenna.Rectangle;
            Rectangle3.Name = "Rectangle3";
            Rectangle3.Center = [0.003 0.074];
            Rectangle3.Length = 0.006;
            Rectangle3.Width = 0.036;
            Rectangle3 = rotate(Rectangle3,0,[Rectangle3.Center,-1],[Rectangle3.Center,1]);
        Rectangle7 = Rectangle7 + Rectangle3;%Add
        %Creating Rectangle7_Copy1 shape.
        Rectangle7_Copy1 = antenna.Rectangle;
        Rectangle7_Copy1.Name = "Rectangle7_Copy1";
        Rectangle7_Copy1.Center = [0.174751 0.089];
        Rectangle7_Copy1.Length = 0.1085;
        Rectangle7_Copy1.Width = 0.006;
        Rectangle7_Copy1 = rotate(Rectangle7_Copy1,0,[Rectangle7_Copy1.Center,-1],[Rectangle7_Copy1.Center,1]);
            %Creating Rectangle3_Copy1 shape.
            Rectangle3_Copy1 = antenna.Rectangle;
            Rectangle3_Copy1.Name = "Rectangle3_Copy1";
            Rectangle3_Copy1.Center = [0.232 0.074];
            Rectangle3_Copy1.Length = 0.006;
            Rectangle3_Copy1.Width = 0.036;
            Rectangle3_Copy1 = rotate(Rectangle3_Copy1,0,[Rectangle3_Copy1.Center,-1],[Rectangle3_Copy1.Center,1]);
        Rectangle7_Copy1 = Rectangle7_Copy1 + Rectangle3_Copy1;%Add
    MetalLayer1 = Rectangle2 + Rectangle7 + Rectangle7_Copy1;
    %Creating DielectricLayer1 dielectric layer.
    DielectricLayer1 = dielectric("Name",'FR4',"EpsilonR",4.8,"LossTangent",0.026,"Thickness",0.0006);

%%Create Feed
feedloc = [[0.1115 0.089 1];...
[0.1235 0.089 1]
    ];

%%Create Metal
metalobj = metal;
metalobj.Name = 'PEC';
metalobj.Conductivity = Inf;
metalobj.Thickness = 0; % 0 mils

pcbobj.Conductor = metalobj;

%%Assign properties
pcbobj.BoardThickness = 0.0006;
pcbobj.Layers = {MetalLayer1,DielectricLayer1,};
pcbobj.FeedLocations = feedloc;
pcbobj.FeedDiameter = 0.001;
pcbobj.ViaDiameter = 0.001;
pcbobj.FeedViaModel = 'strip';
pcbobj.FeedVoltage = 3.3;
pcbobj.FeedPhase = 0;
