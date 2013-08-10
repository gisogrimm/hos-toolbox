function create_onesound( name, d )
  [x,fs] = wavread([name,'.wav']);
  pulsesound(x,fs,[name,'_pulse.wav'],d,true);
