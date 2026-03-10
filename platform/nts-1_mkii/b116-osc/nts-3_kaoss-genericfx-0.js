if (MIDI.isSysEx()) {
    var d = MIDI.getSysExData();
    if (d[5] != 0x40)
      break;
    if (count === undefined)
        count = 0;
    else
        count++;
    var a = [];
    for (var i = 6; i < (576 + 6); i+=8)
        for (var j = 0; j < 7; j++)
            a.push(d[i+j+1] | ((d[i] << (7-j)) & 0x80));
    Util.println(count + ', ' + a[84] + ', ' + a[85] + ', ' + (a[84] + (a[95] << 8)));
}
