## Datasets

### 1998 World-cup server logs

Available [here](ftp://ita.ee.lbl.gov/html/contrib/WorldCup.html).

See data\_proc/world-cup/prepare\_data.sh for how we preprocessed the logs.

Generated files:

    - data/world-cup-98-client-id.txt: (TIMESTAMP, Client-ID) pairs

    - data/world-cup-98-object-id.txt: (TIMESTAMP, Object-ID) pairs

    - data/wc-client-id-attp-w-stats-report.txt: Client-ID data and queries
      for ATTP.

    - data/wc-client-id-bitp-w-stats-report.txt: Client-ID data and queries
      for BITP.

    - data/wc-object-id-attp-w-stats-report.txt: Object-ID data and queries
      for ATTP.

    - data/wc-object-id-bitp-w-stats-report.txt: Object-ID data and queries
      for BITP.

### matrix sketch dataset

run data\_proc/gen\_mat\_data.sh

Generated files:
    
    - data/matrix\_small-w-stats-report.txt: small matrix of d = 100, n = 50000

    - data/matrix\_medium-w-stats-report.txt: matrix of d = 1000, n = 50000

    - data/matrix\_big-w-stats-report.txt: matrix of d = 10000, n = 50000

	- data/ground_truth_XXX.txt: the ground truth data needed for computing the
	  errors

