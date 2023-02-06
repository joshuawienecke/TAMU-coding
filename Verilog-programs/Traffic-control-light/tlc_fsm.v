`timescale 1ns / 1ps


module tlc_fsm(
    output reg [2:0] state,                         ////////////
    output reg RstCount,                            //Outputs
    output reg [1:0] highwaySignal, farmSignal,     ////////////
    input wire [30:0] Count,                        ////////////
    input wire Clk, Rst,                            //Inputs
    input wire farmSensor                           ////////////
    );
    
    parameter     S0 = 3'b001,                   //assigning each state to a binary number 
                  S1 = 3'b010, 
                  S2 = 3'b011, 
                  S3 = 3'b100,
                  S4 = 3'b101,
                  S5 = 3'b110;
    parameter     RED = 2'b01,
                  YELLOW = 2'b10,
                  GREEN = 2'b11;
    initial begin
        state = S0;
    end
    reg [2:0] nextState;                                //Temp reg for next state

    always@(*)
    begin
        case(state)
            //Reset state
            S0:
                begin
                    if(Count >= 50000000) begin        //1 second
                        nextState = S1;
                        RstCount = 1;
                        highwaySignal = GREEN;
                        farmSignal = RED;
                    end
                    else if(Count != 50000000) begin
                        nextState = S0;
                        RstCount = 0;                   //Stay at this state
                        highwaySignal = RED;
                        farmSignal = RED;
                    end
                end
            S1:
                begin
                    if(Count >= 1500000000 && farmSensor) begin        //30 seconds
                        nextState = S2;
                        RstCount = 1;
                        highwaySignal = YELLOW;
                        farmSignal = RED;
                    end
                    else if(Count != 1500000000) begin
                        nextState = S1;
                        RstCount = 0;                   //Stay at this state
                        highwaySignal = GREEN;
                        farmSignal = RED;
                    end
                end
            S2:
                begin
                    if(Count >= 150000000) begin        //3 seconds
                        nextState = S3;
                        RstCount = 1;
                        highwaySignal = RED;
                        farmSignal = RED;
                    end
                    else if(Count != 150000000) begin
                        nextState = S2;
                        RstCount = 0;                   //Stay at this state
                        highwaySignal = YELLOW;
                        farmSignal = RED;
                    end
                end
            S3:
                begin
                    if(Count >= 50000000) begin        //1 second
                        nextState = S4;
                        RstCount = 1;
                        highwaySignal = RED;
                        farmSignal = GREEN;
                    end
                    else if(Count != 50000000) begin
                        nextState = S3;
                        RstCount = 0;                   //Stay at this state
                        highwaySignal = RED;
                        farmSignal = RED;
                    end
                end
            S4:
                begin
                    if(Count >= 750000000) begin        //15 seconds
                        nextState = S5;
                        RstCount = 1;
                        highwaySignal = RED;
                        farmSignal = YELLOW;
                    end
                    else if(Count >= 150000000 && !farmSensor) begin        //15 seconds
                        nextState = S5;
                        RstCount = 1;
                        highwaySignal = RED;
                        farmSignal = YELLOW;
                    end
                    else if(Count != 750000000) begin
                        nextState = S4;
                        RstCount = 0;                   //Stay at this state
                        highwaySignal = RED;
                        farmSignal = GREEN;
                    end
                end
            S5:
                begin
                    if(Count >= 150000000) begin        //3 seconds
                        nextState = S0;
                        RstCount = 1;
                        highwaySignal = RED;
                        farmSignal = RED;
                    end
                    else if(Count != 150000000) begin
                        nextState = S5;
                        RstCount = 0;                   //Stay at this state
                        highwaySignal = RED;
                        farmSignal = YELLOW;
                    end
                end    
        endcase
    end
    
    always @ (posedge Clk) begin
        if(Rst) begin                   //This operates as Srst since the count stays at zero as long as
            state <= S0;                //Set state back to reset state
            end                 
        else
            state <= nextState;         //Progress to next state (might be the same state)
    end

endmodule