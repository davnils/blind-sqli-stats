#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include <random>
#include <vector>

typedef double fp_t;

const unsigned int BOOTSTRAP_SAMPLES = 10000;
const fp_t         SIGNIFICANCE_ALPHA = 0.01;
const unsigned int INITAL_SAMPLE_SIZE = 4U;
const unsigned int MAX_SAMPLE_SIZE = 60U;

std::mt19937 g_rng;

/*void dump(const std::vector<fp_t> & vec)
{
  for(auto v : vec)
  {
    std::cerr << v << " ";
  }
  std::cerr << std::endl;
}*/

fp_t average(const std::vector<fp_t> & values)
{
  fp_t acc = 0.0f;

  for(auto val : values)
  {
    acc += val;
  }

  return(acc / values.size());
}

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
 * Can be improved by implementing the BC_a method for confidence intervals.
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

  auto x = readSamples(samples.first, INITAL_SAMPLE_SIZE);
  auto y = readSamples(samples.second, INITAL_SAMPLE_SIZE);

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
