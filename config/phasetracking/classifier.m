function [vClass,vP] = classifier( mParam, sTable, sParam )
% CLASSIFIER - simple classifier with onedimensional bayes tables
%
% Usage: 
% vClass = classifier( mParam, sTable, sParam );
% 
% mParam : Matrix with observation data, one row per sample
% sTable : Bayes table structure, created by classifier_create()
% sParam : 'prod' (default) or 'mean'
  
  if nargin < 3
    sParam = 'prod';
  end
  nParams = size(mParam,2);
  nClasses = size(sTable.tab,3);
  mP = zeros(size(mParam,1),nParams,nClasses);
  for k=1:nParams
    mP(:,k,:) = interp1( sTable.edges(:,k), squeeze( sTable.tab(:,k,:) ), ...
		  mParam(:,k) );
  end
  if( strcmp(sParam,'prod'))
    vP = prod(mP,2);
  else
    vP = mean(mP,2);
  end
  vP = squeeze(vP);
  [tmp,vClass] = max(vP,[],2);