#include "shared/utils.h"
#include "shared/most_common.h"
#include "shared/lrs_test.h"
#include "non_iid/collision_test.h"
#include "non_iid/lz78y_test.h"
#include "non_iid/multi_mmc_test.h"
#include "non_iid/lag_test.h"
#include "non_iid/multi_mcw_test.h"
#include "non_iid/compression_test.h"
#include "non_iid/markov_test.h"

#include <getopt.h>
#include <limits.h>

#include <algorithm>


[[ noreturn ]] void print_usage() {
	printf("Usage is: ea_non_iid [-i|-c] [-a|-t] [-v] [-l <index>,<samples> ] <file_name> [bits_per_symbol]\n\n");
	printf("\t <file_name>: Must be relative path to a binary file with at least 1 million entries (samples).\n");
	printf("\t [bits_per_symbol]: Must be between 1-8, inclusive. By default this value is inferred from the data.\n");
	printf("\t [-i|-c]: '-i' for initial entropy estimate, '-c' for conditioned sequential dataset entropy estimate. The initial entropy estimate is the default.\n");
	printf("\t [-a|-t]: '-a' tests all bits in bitstring, '-t' truncates bitstring to %d bits. Test all data by default.\n", MIN_SIZE);
	printf("\t -v: Optional verbosity flag for more output. Can be used multiple times.\n");
	printf("\t -l <index>,<samples>\tRead the <index> substring of length <samples>.\n");
	printf("\n");
	printf("\t Samples are assumed to be packed into 8-bit values, where the least significant 'bits_per_symbol'\n");
	printf("\t bits constitute the symbol.\n"); 
	printf("\n");
	printf("\t -i: Initial Entropy Estimate (Section 3.1.3)\n");
	printf("\n");
	printf("\t\t Computes the initial entropy estimate H_I as described in Section 3.1.3\n");
	printf("\t\t (not accounting for H_submitter) using the entropy estimators specified in\n");
	printf("\t\t Section 6.3.  If 'bits_per_symbol' is greater than 1, the samples are also\n");
	printf("\t\t converted to bitstrings and assessed to create H_bitstring; for multi-bit symbols,\n");
	printf("\t\t two entropy estimates are computed: H_original and H_bitstring.\n");
	printf("\t\t Returns min(H_original, bits_per_symbol X H_bitstring). The initial entropy\n");
	printf("\t\t estimate H_I = min(H_submitter, H_original, bits_per_symbol X H_bitstring).\n");
	printf("\n");
	printf("\t -c: Conditioned Sequential Dataset Entropy Estimate (Section 3.1.5.2)\n");
	printf("\n");
	printf("\t\t Computes the entropy estimate per bit h' for the conditioned sequential dataset if the\n");
	printf("\t\t conditioning function is non-vetted. The samples are converted to a bitstring.\n");
	printf("\t\t Returns h' = min(H_bitstring).\n");
	printf("\n");
	exit(-1);
}

double estimate(const data_t& data, int verbose, bool useBinary)
{
	byte* symbols = useBinary ? data.bsymbols : data.symbols;
	long len = useBinary ? data.blen : data.len;
	int alpha_size = useBinary ? 2 : data.alph_size;
	const char* label = useBinary ? "Bitstring" : "Literal";

	std::vector<double> entropies;

	auto tmp1 = 0.0;
	auto tmp2 = 0.0;

	if(verbose <= 1) {
		printf("\nRunning non-IID tests...\n\n");
		printf(">> Running Most Common Value Estimate...\n");
	}
	entropies.push_back(most_common(symbols, len, alpha_size, verbose, label));

	if(alpha_size == 2) {
		if(verbose <= 1) printf("\n>> Running Entropic Statistic Estimates (bit strings only)...\n");
		entropies.push_back(collision_test(symbols, len, verbose, label));
		entropies.push_back(markov_test(symbols, len, verbose, label));
		entropies.push_back(compression_test(symbols, len, verbose, label));
	}

	if(verbose <= 1) printf("\n>> Running Tuple Estimates...\n");	
	SAalgs(symbols, len, alpha_size, tmp1, tmp2, verbose, label);
	entropies.push_back(tmp1);
	entropies.push_back(tmp2);

	if(verbose <= 1) printf("\n>> Running Predictor Estimates...\n");
	entropies.push_back(multi_mcw_test(symbols, len, alpha_size, verbose, label));
	entropies.push_back(lag_test(symbols, len, alpha_size, verbose, label));
	entropies.push_back(multi_mmc_test(symbols, len, alpha_size, verbose, label));
	entropies.push_back(LZ78Y_test(symbols, len, alpha_size, verbose, label));

	return *std::min_element(entropies.begin(), entropies.end());
}

int main(int argc, char* argv[]){
	bool initial_entropy, all_bits;
	int verbose = 0;
	char *file_path;
	double H_original, H_bitstring, ret_min_entropy; 
	data_t data;
	int opt;
	double bin_t_tuple_res = -1.0, bin_lrs_res = -1.0;
	double t_tuple_res = -1.0, lrs_res = -1.0;
	unsigned long subsetIndex=ULONG_MAX;
	unsigned long subsetSize=0;
	unsigned long long inint;
	char *nextOption;

	data.word_size = 0;

	initial_entropy = true;
	all_bits = true;

	while ((opt = getopt(argc, argv, "icatvl:")) != -1) {
		switch(opt) {
			case 'i':
					initial_entropy = true;
					break;
			case 'c':
					initial_entropy = false;
					break;
			case 'a':
					all_bits = true;
					break;
			case 't':
					all_bits = false;
					break;
			case 'v':
					verbose++;
					break;
			case 'l':
				inint = strtoull(optarg, &nextOption, 0);
				if((inint > ULONG_MAX) || (errno == EINVAL) || (nextOption == NULL) || (*nextOption != ',')) {
					print_usage();
				}
				subsetIndex = inint;

				nextOption++;

				inint = strtoull(nextOption, NULL, 0);
				if((inint > ULONG_MAX) || (errno == EINVAL)) {
					print_usage();
				}
				subsetSize = inint;
				break;
			default:
					print_usage();
			}
	}

	argc -= optind;
	argv += optind;

	// Parse args
	if((argc != 1) && (argc != 2)){
		printf("Incorrect usage.\n");
		print_usage();
	} 
	// get filename
	file_path = argv[0];

	if(argc == 2) {
		// get bits per word
		inint = atoi(argv[1]);
		if(inint < 1 || inint > 8){
			printf("Invalid bits per symbol.\n");
			print_usage();
		} else {
			data.word_size = inint;
		}
	}

	if(verbose>0) printf("Opening file: '%s'\n", file_path);

	if(!read_file_subset(file_path, &data, subsetIndex, subsetSize)){
		printf("Error reading file.\n");
		print_usage();
	}
	if(verbose > 0) printf("Loaded %ld samples of %d distinct %d-bit-wide symbols\n", data.len, data.alph_size, data.word_size);

	if(data.alph_size <= 1){
		printf("Symbol alphabet consists of 1 symbol. No entropy awarded...\n");
		free_data(&data);
		exit(-1);
	}

	if(!all_bits && (data.blen > MIN_SIZE)) data.blen = MIN_SIZE;
	if(!all_bits && (data.len > MIN_SIZE)) data.len = MIN_SIZE;

	if((verbose>0) && ((data.alph_size > 2) || !initial_entropy)) printf("Number of Binary Symbols: %ld\n", data.blen);
	if(data.len < MIN_SIZE) printf("\n*** Warning: data contains less than %d samples ***\n\n", MIN_SIZE);
	if(verbose>0){
		if(data.alph_size < (1 << data.word_size)) printf("\nSymbols have been translated.\n");
	}

	// The maximum min-entropy is -log2(1/2^word_size) = word_size
	// The maximum bit string min-entropy is 1.0
	H_original = data.word_size;
	H_bitstring = 1.0;

	if(((data.alph_size > 2) || !initial_entropy)) {
		H_bitstring = min(H_bitstring, estimate(data, verbose, true));
	}

	if(initial_entropy) {
		H_original = min(H_original, estimate(data, verbose, false));
	}

	if(verbose <= 1) {
		printf("\n");
		if(initial_entropy){
			printf("H_original: %f\n", H_original);
			if(data.alph_size > 2) {
				printf("H_bitstring: %f\n\n", H_bitstring);
				printf("min(H_original, %d X H_bitstring): %f\n\n", data.word_size, min(H_original, data.word_size*H_bitstring));
			}
		} else printf("h': %f\n", H_bitstring);
	} else {
		double h_assessed = data.word_size;

		if((data.alph_size > 2) || !initial_entropy) {
			h_assessed = min(h_assessed, H_bitstring * data.word_size);
			printf("H_bitstring = %.17g\n", H_bitstring);
		}

		if(initial_entropy) {
			h_assessed = min(h_assessed, H_original);
			printf("H_original: %.17g\n", H_original);
		}

		printf("Assessed min entropy: %.17g\n", h_assessed);
	}

	free_data(&data);
	return 0;
}
