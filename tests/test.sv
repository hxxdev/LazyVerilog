module memory(
    address,
    data_in,
    data_out,
    read_write,
    chip_en
  );

  input wire [7:0] address, data_in;
  output reg [7:0] data_out;
  input wire read_write, chip_en;

  reg [7:0] mem [0:255];

  always @ (address or data_in or read_write or chip_en)
    if (read_write == 1 && chip_en == 1) begin
      mem[address] = data_in;
  end

  always @ (read_write or chip_en or address)
    if (read_write == 0 && chip_en)
      data_out = mem[address];
    else
      data_out = 0;

endmodule
