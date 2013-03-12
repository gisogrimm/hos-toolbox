function plot_phases( kP )
  csN = {'julia','giso','claire'};
  for k=1:3
    s(k)=load(['ph_',csN{k},'.mat']);
  end
  for k=1:3
    s(k).c = s(k).s+i*s(k).gf_imag;
  end
  size(s(1).c)
  vT = [1:size(s(1).c,1)]/251;
  c0 = conj(s(1).c(:,2));
  %c0 = exp(-i*2*pi*vT');
  plot(vT,0.5/pi*(unwrap(angle([s(1).c(:,kP).*c0,s(2).c(:,kP).*c0,s(3).c(:,kP).*c0]))));
  grid on
  legend(csN);