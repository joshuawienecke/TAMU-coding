`timescale 1ns / 1ps


module tlc_controller_verl(
    output wire [1:0] highwaySignal, farmSignal,                           
    output wire [3:0] JB,                                                  
    input wire Clk,                                                        
    input wire Rst,                                                         
    input wire farmSensor                                                   
);
    wire RstSync;                                                          
    wire RstCount;                                                          
    wire farmSensorSync;                                                   
    reg [30:0] Count = 0;     // 30 bit count inititalized to 0                                           
    
    assign JB[3] = RstCount;                                              
    
    // syncrhonize the outputs to prevent bouncing
    synchronizer syncRst(RstSync, Rst, Clk);                             
    synchronizer syncfarmSensor(farmSensorSync, farmSensor, Clk);          
    
    // instantiate FSM
    tlc_fsm FSM(                                                    
        .state(JB[2:0]), 
        .RstCount(RstCount), 
        .highwaySignal(highwaySignal), 
        .farmSignal(farmSignal), 
        .Count(Count), 
        .Clk(Clk), 
        .Rst(RstSync),
        .farmSensor(farmSensorSync)); // add in farm sensor
        
    always@(posedge Clk)begin // count with each clk edge
        if(RstCount)                                                        
            Count = 0;// reset to 0 with RstCount                                                     
        else
            Count = Count + 1; // up count by one up to 30                                            
    end
endmodule