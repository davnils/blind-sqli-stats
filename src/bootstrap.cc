/**
 * Bootstrap implementation written for timing-based blind sql injections.
 *
 * Author: David Nilsson
 */
#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include <random>
#include <vector>

typedef double fp_t;

const unsigned int BOOTSTRAP_SAMPLES = 10000; /* Number of samples drawn from data set. */
const fp_t         SIGNIFICANCE_ALPHA = 0.01; /* alpha parameter in confidence interval.  */
const unsigned int INITAL_SAMPLE_SIZE = 4U;   /* Minimum number of samples from each group. */
const unsigned int MAX_SAMPLE_SIZE = 60U;     /* Maximum numer of samples from each group. */

/* Global PRNG seeded once. */
std::mt19937 g_rng;

/**
 * Calculate average of a vector of values.
 *
 * @param values Vector to be averaged.
 * @return Arithmetic mean value.
 */
fp_t average(const std::vector<fp_t> & values)
{
  fp_t acc = 0.0f;

  for(auto val : values)
  {
    acc += val;
  }

  return(acc / values.size());
}

/**
 * Create a sample of size 'sampleSize' drawn from 'values'.
 * All elements are drawn uniformly with replacement.
 *
 * @param values Source vector.
 * @param sampleSize Number of elements in resulting vector.
 * @return Uniform sample from input vector.
 */
std::vector<fp_t> sample(const std::vector<fp_t> & values, const unsigned int sampleSize)
{
  std::vector<fp_t> result;
  result.reserve(sampleSize);

  std::uniform_int_distribution<size_t> dist(0, values.size() - 1);

  for(auto i = 0U; i < sampleSize; i++)
  {
    result.push_back(values[dist(g_rng)]);
  }

  return(result);
}

/**
 * Calculate confidence interval using the percentile method.
 *
 * @param values Ordered set of values.
 * @param alpha Significance level. 
 * @return Lower and upper bound of (alpha) confidence interval.
 */
std::pair<fp_t, fp_t> getInterval(const std::vector<fp_t> & values, const fp_t alpha)
{
  fp_t halfAlpha = alpha/2;
  fp_t lowerIndex = (values.size() - 1) * halfAlpha;
  fp_t upperIndex = (values.size() - 1) * (1 - halfAlpha);

  //Conservative rounding
  return(std::make_pair(
          values.at(std::floor(lowerIndex)),
          values.at(std::ceil(upperIndex)))
  );
}

/**
 * Null hypothesis testing using bootstrap and percentile confidence intervals.
 *
 * Takes two input vectors and tries to reject the null hypothesis:
 *
 * H_0: 'The distributions share the same parameter'
 *
 * Or in other words; one of the vectors do not represent timing associated
 * with a successful blind sql injection.
 *
 * Ordering of input vectors does not matter.
 *
 * @param x First vector of values.
 * @param y Second vector of values.
 * @return True if H_0 was rejected at the alpha significance level.
 */
bool rejected(const std::vector<fp_t> & x, const std::vector<fp_t> & y)
{
  std::vector<fp_t> bootStrapped;
  bootStrapped.reserve(BOOTSTRAP_SAMPLES);

  for(auto b = 0U; b < BOOTSTRAP_SAMPLES; b++)
  {
    auto xSample = sample(x, x.size());
    auto ySample = sample(y, y.size());

    bootStrapped.push_back(average(xSample) - average(ySample));
  }

  std::sort(std::begin(bootStrapped), std::end(bootStrapped));

  auto bounds = getInterval(bootStrapped, SIGNIFICANCE_ALPHA);

  return(bounds.first > 0 || bounds.second < 0);
}

/**
 * Parse stdin into two separate vectors of values.
 * Format:
 *
 * <n>
 * x_1 x_2 x_3 ... x_n
 * y_1 y_2 y_3 ... y_n
 *
 * @return Pair of equally long vectors containing all the parsed input.
 */
std::pair<std::deque<fp_t>, std::deque<fp_t>> parseInput()
{
  unsigned int numValues;
  std::string line;

  while(std::getline(std::cin, line) && line.at(0) == '#');
  numValues = atoi(line.c_str());

  std::deque<fp_t> reference, offset;

  auto fill = [numValues](std::deque<fp_t> & q)
  {
    for(auto i = 0U; i < numValues; i++)
    {
      fp_t tmp;
      std::cin >> tmp;
      q.push_back(tmp);
    }
  };

  fill(reference);
  fill(offset);

  return(std::make_pair(reference, offset));
}

/**
 * Read a number of samples from the supplied input.
 *
 * This function should in reality generate asynchronous HTTP requests
 * and register the round-trip-time.
 *
 * @param input Vector to be read.
 * @param n Number of samples to be acquired.
 * @return Vector of registered samples.
 */
std::vector<fp_t> readSamples(std::deque<fp_t> & input, unsigned int n)
{
  assert(input.size() >= n && n != 0);
  std::vector<fp_t> result;
  result.reserve(n);

  for(auto i = 0U; i < n; i++)
  {
    result.push_back(input.front());
    input.pop_front();
  }

  return(result);
}

int main()
{

  std::random_device device;
  g_rng = std::mt19937(device());

  auto samples = parseInput();
  assert(samples.first.size() >= INITAL_SAMPLE_SIZE &&
         samples.first.size() >= MAX_SAMPLE_SIZE);

  /* Read initial batch size. */
  auto x = readSamples(samples.first, INITAL_SAMPLE_SIZE);
  auto y = readSamples(samples.second, INITAL_SAMPLE_SIZE);

  /**
   * Read a new set of samples until either:
   * (1) The null hypothesis is rejected
   * (2) The limit is reached
   */
  auto current = INITAL_SAMPLE_SIZE;
  while(current <= MAX_SAMPLE_SIZE)
  {
    if(rejected(x, y))
    {
      std::cerr << "Null hypothesis rejected: blind sql injection highly likely"
                << std::endl;
      return(1);
    }

    x.insert(std::end(x), readSamples(samples.first, 1).front());
    y.insert(std::end(y), readSamples(samples.second, 1).front());
    current++;
  }

  std::cerr << "Null hypothesis not rejected" << std::endl;
  return(0);
}
