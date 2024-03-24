#!/usr/bin/wish

wm title . "IRC"
pack [ frame .f ]
pack [ listbox .f.l -yscrollcommand { .f.s set }  ] -side left -fill both
pack [ scrollbar .f.s -command { .f.l yview }  ] -side left -fill y
pack [ entry .e -textvariable linha ] -fill x

set fd [ open "| ./cli" r+ ]
set fr [ open log a+]

bind      .e  <Key-Return> "envia $fd"
fileevent $fr readable     "recebe $fr"



proc envia {fd} {
    #"recebe $fr"
    global linha
    global env

    puts $fd "$linha"
    flush $fd

    set linha ""

}
proc recebe {fd} {
    while { [ gets $fd linha ] > -1 } {
        .f.l insert end $linha
        .f.l see end
    }
    update idletasks
}


