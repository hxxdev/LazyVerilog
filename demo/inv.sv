module inv(
    i_a, o_d
);
`include "params.svh"
input   fifo_entry_t        [3:0]               i_a                                     ;
output  fifo_entry_t        [3:0]               o_d                                     ;

assign i_d  = ~i_a;

endmodule
