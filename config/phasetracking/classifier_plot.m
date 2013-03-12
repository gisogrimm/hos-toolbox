function classifier_plot( sCf )
  nFeatures = numel(sCf.labels);
  nClasses = size( sCf.tab,3 );
  for k = 1:nFeatures
    fh = figure('Name',sprintf('classifier, feature %s',sCf.labels{k}) );
    subplot(2,1,1);
    plot(sCf.edges(:,k),squeeze(sCf.hist(:,k,:)));
    xlim([sCf.min(k),sCf.max(k)]);
    ylabel('rel. frequency');
    subplot(2,1,2);
    plot(sCf.edges(:,k),squeeze(sCf.tab(:,k,:)));
    xlim([sCf.min(k),sCf.max(k)]);
    xlabel(sCf.labels{k},'interpreter','none');
    ylabel('Bayes prob.');
    legend(num2str([1:nClasses]'));
  end
  