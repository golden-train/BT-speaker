#include "storage/assets.h"
#include "storage/sd_card.h"

namespace assets {

Report scan() {
  Report r{};
  if (!sd_card::isMounted()) return r;
  r.mounted    = true;
  r.totalKB    = sd_card::totalKB();
  r.usedKB     = sd_card::usedKB();
  r.font16Size = sd_card::fileSize(kFont16);
  r.font12Size = sd_card::fileSize(kFont12);
  r.animFrames = sd_card::countFiles(kAnimDir);
  return r;
}

}  // namespace assets
