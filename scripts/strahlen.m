function strahlen( N )
  if nargin < 1
    N = 24;
  end
  figure
  vAz = [0:(N-1)]/N*360;
  for k=1:numel(vAz)
    c = [0.2,1]*exp(i*pi*vAz(k)/180);
    plot(real(c),imag(c),'k-','linewidth',2);
    hold on;
    text(real(1.1*c(2)),imag(1.1*c(2)),sprintf('%d',k),...
	 'HorizontalAlignment','center');
  end
  plot([-0.1,0.1],[0,0],'k-');
  plot([0,0],[-0.1,0.1],'k-');
  set(gca,'DataAspectRatio',[1,1,1],'visible','off');
  saveas(gcf,'strahlen.eps','eps');
  system('epstopdf strahlen.eps');