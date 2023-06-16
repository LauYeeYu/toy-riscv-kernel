    .section .data
    .global init_program
init_program:
    .incbin "user/bin/init"

    .section .data
    .global sh
sh:
    .incbin "user/bin/sh"

    .section .data
    .global echo
echo:
    .incbin "user/bin/echo"
