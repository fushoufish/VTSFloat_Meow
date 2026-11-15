    enum class AnrPresentation { Immediate, DelayedReal, Generated };
    struct AnrPerformanceSample {
        double renderedFps;
        double outputFps;
        std::optional<double> extraMs;
    };
    static constexpr int kDefaultAnrLatencyLimitMs = 0;
    static constexpr int kAnrLatencyLimits[] = {0, 10, 20, 30, 40, 60, 100};

    int ReadAnrLatencyLimit() const {
        const int saved = ReadConfigInt(L"anr", L"extra_latency_limit_ms", kDefaultAnrLatencyLimitMs);
        for (const int limit : kAnrLatencyLimits) {
            if (saved == limit) return limit;
        }
        return kDefaultAnrLatencyLimitMs;
    }

    void SetAnrLatencyLimit(int limit) {
        if (anrLatencyLimitMs_ == limit) return;
        anrLatencyLimitMs_ = limit;
        WriteConfigInt(L"anr", L"extra_latency_limit_ms", limit);
        ResetAnrFrames();
        resetFrameSchedule_ = true;
        Log("[anr-fg] latency_limit_ms=" + std::to_string(limit));
    }

    int EffectiveAnrMultiplier() const { return (std::min)(anrMultiplier_, anrProviderMaxMultiplier_); }

    void SetAnrMultiplier(int multiplier) {
        if (multiplier == anrMultiplier_ || multiplier > anrProviderMaxMultiplier_) return;
        anrMultiplier_ = multiplier;
        WriteConfigInt(L"anr", L"multiplier", multiplier);
        ResetAnrFrames();
        resetFrameSchedule_ = true;
        Log("[anr-fg] multiplier=" + std::to_string(multiplier));
    }

    static std::filesystem::path AnrEnvironmentPath(const wchar_t* key) {
        const DWORD size = GetEnvironmentVariableW(key, nullptr, 0);
        if (!size) return {};
        std::wstring value(size, L'\0');
        const DWORD written = GetEnvironmentVariableW(key, value.data(), size);
        if (!written || written >= size) return {};
        value.resize(written);
        return value;
    }

    void ShowAnrDownloadNotice(std::wstring text,
        UpdateNoticeDismissal dismissal = UpdateNoticeDismissal::Manual) {
        if (updateBannerText_ == text && updateNoticeStyle_ == UpdateNoticeStyle::AnrDownload &&
            updateNoticeDismissal_ == dismissal) return;
        ShowUpdateBanner(std::move(text), UpdateNoticeStyle::AnrDownload, dismissal);
    }

    void StartAnrDownload() {
        if (anrDownload_.valid()) return;
        anrDownloadProgress_.cancelled = false;
        anrDownloadProgress_.installing = false;
        anrDownloadProgress_.downloaded = 0;
        anrDownloadSampleAt_ = Clock::now();
        anrDownloadSampleBytes_ = 0;
        try {
            anrDownload_ = std::async(std::launch::async,
                [this] { return vtsfloat::InstallAnrPackage(anrDownloadProgress_); });
        } catch (const std::exception& error) {
            Log(std::string("[anr-fg] start download failed: ") + error.what());
            ShowAnrDownloadNotice(Tr(L"ANR FG 下载失败，请重试。"));
            return;
        }
        Log("[anr-fg] downloading FG V.B15");
        SetTimer(hwnd_, kAnrDownloadTimer, 250, nullptr);
        ShowAnrDownloadNotice(std::wstring(Tr(L"正在下载 ANR FG…")) + L" 0%  0.0 KiB/s");
    }

    void PollAnrDownload() {
        if (!anrDownload_.valid()) return;
        if (anrDownload_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            if (anrDownloadProgress_.cancelled) {
                ShowAnrDownloadNotice(Tr(L"正在取消 ANR FG 下载…"), UpdateNoticeDismissal::None);
            } else if (anrDownloadProgress_.installing) {
                ShowAnrDownloadNotice(Tr(L"正在安装 ANR FG…"), UpdateNoticeDismissal::None);
            } else {
                const auto now = Clock::now();
                const auto bytes = anrDownloadProgress_.downloaded.load();
                const auto elapsed = std::chrono::duration<double>(now - anrDownloadSampleAt_).count();
                const auto delta = bytes >= anrDownloadSampleBytes_ ? bytes - anrDownloadSampleBytes_ : bytes;
                const double kibPerSecond = elapsed > 0 ? delta / (elapsed * 1024.0) : 0.0;
                anrDownloadSampleAt_ = now;
                anrDownloadSampleBytes_ = bytes;
                const bool mebibytes = kibPerSecond >= 1024.0;
                std::wostringstream notice;
                notice << Tr(L"正在下载 ANR FG…") << L" " << bytes * 100 / vtsfloat::kAnrPackageSize
                       << L"%  " << std::fixed << std::setprecision(1)
                       << (mebibytes ? kibPerSecond / 1024.0 : kibPerSecond)
                       << (mebibytes ? L" MiB/s" : L" KiB/s");
                ShowAnrDownloadNotice(notice.str());
            }
            return;
        }

        KillTimer(hwnd_, kAnrDownloadTimer);
        try {
            const auto path = anrDownload_.get();
            if (anrDownloadProgress_.cancelled) {
                ShowUpdateBanner({});
            } else if (StartAnrProvider(path)) {
                ShowAnrDownloadNotice(Tr(L"ANR FG 下载完成并已启用"), UpdateNoticeDismissal::Timed);
            }
        } catch (const std::exception& error) {
            if (anrDownloadProgress_.cancelled) {
                ShowUpdateBanner({});
            } else {
                Log(std::string("[anr-fg] download failed: ") + error.what());
                ShowAnrDownloadNotice(Tr(L"ANR FG 下载失败，请重试。"));
            }
        }
        // Restore any update offer temporarily covered by the download.
        if (updater_ && !updateDismissed_ && updater_->Poll().phase == vtsfloat::update::Phase::Available) {
            updateRevision_ = 0;
        }
        PollUpdate();
    }

    bool StartAnrProvider(const std::filesystem::path& path) {
        std::error_code fileError;
        auto provider = std::filesystem::absolute(path, fileError);
        if (fileError || _wcsicmp(provider.filename().c_str(), L"nvngx_dlssg.dll") != 0 ||
            !std::filesystem::is_regular_file(provider, fileError) ||
            !std::filesystem::is_regular_file(provider.parent_path() / L"path_table.json", fileError)) {
            Log("[anr-fg] provider files are missing");
            ShowAnrDownloadNotice(Tr(L"ANR FG 无法启动，请查看日志。"));
            return false;
        }
        try {
            anrStream_ = std::make_unique<vtsfloat::AnrFrameStream>(std::move(provider));
        } catch (const std::exception& error) {
            Log(std::string("[anr-fg] ") + error.what());
            ShowAnrDownloadNotice(Tr(L"ANR FG 无法启动，请查看日志。"));
            return false;
        }
        anrAdapterLogged_ = false;
        anrPresentationCount_ = 0;
        ResetAnrFrames();
        resetFrameSchedule_ = true;
        Log("[anr-fg] enabled");
        return true;
    }

    void SetAnrEnabled(bool enabled) {
        if (!enabled) {
            if (anrDownload_.valid()) anrDownloadProgress_.cancelled = true;
            anrStream_.reset();
            ResetAnrFrames();
            if (std::exchange(anrPerformance_, std::nullopt) && toolbarHwnd_) {
                InvalidateRect(toolbarHwnd_, nullptr, FALSE);
            }
            resetFrameSchedule_ = true;
            Log("[anr-fg] disabled");
            return;
        }
        if (!useDx12_ || !vtsfloat::AnrFrameStream::Built()) return;
        auto path = AnrEnvironmentPath(L"VTSFLOAT_ANR_PROVIDER");
        if (path.empty()) path = vtsfloat::CachedAnrProvider();
        if (path.empty()) StartAnrDownload();
        else StartAnrProvider(path);
    }

    void ResetAnrFrames() {
        if (anrStream_) anrStream_->Reset();
        anrGeneratedFrames_.clear();
        anrCaptureDue_ = anrGeneratedDue_ = anrLastOutput_ = {};
        anrLastPresentedCapture_ = {};
        anrBudgetFallback_ = false;
        anrInterpolatedGroup_ = false;
        ResetAnrStats();
    }

    int ScheduledRenderFps() const {
        const int source = EffectiveRequestFps();
        return CanRenderAnr() ? (std::min)(kMaximumFps, source * EffectiveAnrMultiplier()) : source;
    }

    bool CanRenderAnr() const {
        return anrStream_ && !cropEditing_ && !interactiveRendering_.load(std::memory_order_relaxed);
    }

    Clock::duration AnrCaptureInterval() const {
        return std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(1.0 /
                (std::min)(EffectiveRequestFps(), kMaximumFps / EffectiveAnrMultiplier())));
    }

    bool ShouldCaptureAnr(Clock::time_point now) {
        if (now + std::chrono::milliseconds(1) < anrCaptureDue_) return false;
        const auto interval = AnrCaptureInterval();
        if (anrCaptureDue_ == Clock::time_point{}) anrCaptureDue_ = now;
        const auto missed = (std::max)(Clock::duration::zero(), now - anrCaptureDue_) / interval;
        anrCaptureDue_ += (missed + 1) * interval;
        return true;
    }

    // Hold the real frame until the next input arrives so all interpolated
    // frames can be displayed in timestamp order.
    bool SelectAnrOutput(int width, int height, bool captured, Clock::time_point& captureTime,
        AnrPresentation& frameKind, double& extraMs) {
        const auto now = Clock::now();
        const auto budget = std::chrono::milliseconds(anrLatencyLimitMs_);
        if (!anrGeneratedFrames_.empty() &&
            (anrQueuedSize_.cx != width || anrQueuedSize_.cy != height)) {
            anrGeneratedFrames_.clear();
        }
        const vtsfloat::AnrFrame* output = nullptr;
        vtsfloat::AnrFramePair pair;
        // A group that has already missed every presentation slot must
        // not delay a newer source pair. If no new pair is available, the
        // newest due interpolation can still fill the current slot.
        const bool groupOverdue = anrInterpolatedGroup_ && !anrGeneratedFrames_.empty() &&
            now >= anrGeneratedDue_ + anrGeneratedStep_ * static_cast<int>(anrGeneratedFrames_.size());
        if ((anrGeneratedFrames_.empty() || groupOverdue) && anrStream_->Take(pair) &&
            pair.real.width == width && pair.real.height == height) {
            anrGeneratedFrames_.clear();
        }
        if (!anrGeneratedFrames_.empty()) {
            while (anrGeneratedFrames_.size() > 1 && now >= anrGeneratedDue_ + anrGeneratedStep_) {
                anrGeneratedFrames_.pop_front();
                anrGeneratedDue_ += anrGeneratedStep_;
            }
            const auto& frame = anrGeneratedFrames_.front();
            if (anrLatencyLimitMs_ && now - frame.ready > budget) {
                anrGeneratedFrames_.clear();
                ++anrBudgetDrops_;
                anrBudgetFallback_ = true;
            } else if (now + std::chrono::milliseconds(1) >= anrGeneratedDue_) {
                output = &frame;
                frameKind = AnrPresentation::Generated;
            }
        } else if (!pair.generated.empty() && pair.real.width == width && pair.real.height == height) {
            const auto count = static_cast<int>(pair.generated.size());
            const auto step = (std::min)(pair.interval, AnrCaptureInterval()) / (count + 1);
            if (anrLatencyLimitMs_ && (now - pair.real.ready > budget ||
                now + step * count - pair.generated.back().ready > budget)) {
                ++anrBudgetDrops_;
                anrBudgetFallback_ = true;
            } else if (pair.real.captured > anrLastPresentedCapture_) {
                output = &pair.real;
                frameKind = AnrPresentation::DelayedReal;
                anrGeneratedDue_ = now + step;
                anrGeneratedStep_ = step;
                anrQueuedSize_ = {width, height};
                anrInterpolatedGroup_ = false;
                anrProcessingMs_ += pair.processingMs;
                if (++anrPresentationCount_ == 12) SaveAnrSample(pair.real, pair.generated[count / 2]);
                for (auto& generated : pair.generated) anrGeneratedFrames_.push_back(std::move(generated));
            } else {
                // An immediate frame is already newer. Resume buffering
                // without showing an older frame on recovery.
                anrBudgetFallback_ = false;
                anrLastOutput_ = now;
            }
        }
        if (!output) {
            return captured && (anrBudgetFallback_ || now - anrLastOutput_ >= std::chrono::milliseconds(100));
        }
        anrBudgetFallback_ = false;
        captureTime = output->captured;
        extraMs = std::chrono::duration<double, std::milli>(now - output->ready).count();
        vtsfloat::ResizeAnrFrame(*output, width, height, dibBits_);
        if (frameKind == AnrPresentation::Generated) {
            anrGeneratedFrames_.pop_front();
            anrGeneratedDue_ += anrGeneratedStep_;
            anrInterpolatedGroup_ = true;
        }
        anrLastOutput_ = now;
        return true;
    }

    void SaveAnrSample(const vtsfloat::AnrFrame& real, const vtsfloat::AnrFrame& generated) const {
        std::error_code ignored;
        if (anrSampleDirectory_.empty() || !std::filesystem::is_directory(anrSampleDirectory_, ignored)) return;
        std::ofstream sizes(anrSampleDirectory_ / L"size.txt");
        sizes << real.width << ' ' << real.height << '\n';
        sizes << generated.width << ' ' << generated.height << '\n';
        std::ofstream source(anrSampleDirectory_ / L"real.bgra", std::ios::binary);
        source.write(reinterpret_cast<const char*>(real.pixels.data()), real.pixels.size());
        std::ofstream midpoint(anrSampleDirectory_ / L"generated.bgra", std::ios::binary);
        std::vector<std::uint8_t> pixels(real.pixels.size());
        vtsfloat::ResizeAnrFrame(generated, real.width, real.height, pixels.data());
        midpoint.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    }

    void RecordAnrPresentation(AnrPresentation frameKind, Clock::time_point captured, double extraMs) {
        const double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - captured).count();
        const auto index = static_cast<size_t>(frameKind);
        ++anrPresented_[index];
        anrPresentationMs_[index] += elapsed;
        anrLastPresentedCapture_ = captured;
        if (frameKind != AnrPresentation::Immediate) {
            ++anrExtraSamples_;
            anrExtraMs_ += extraMs;
            anrPeakExtraMs_ = (std::max)(anrPeakExtraMs_, extraMs);
        }
    }

    void DrawAnrPerformanceStatus(HDC dc, RECT bounds, const std::wstring& status) const {
        std::wostringstream original, interpolated;
        original << std::fixed << std::setprecision(1) << L"FPS " << anrPerformance_->renderedFps;
        interpolated << std::fixed << std::setprecision(1) << L"ANR " << anrPerformance_->outputFps << L" FPS";
        const auto extent = [&](const std::wstring& label) {
            SIZE size{};
            GetTextExtentPoint32W(dc, label.c_str(), static_cast<int>(label.size()), &size);
            return size;
        };
        constexpr UINT flags = DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
        constexpr COLORREF orange = RGB(255, 177, 88);
        const LONG gap = UiScaled(7);
        const LONG inset = UiScaled(4);
        const LONG lineHeight = extent(L"FPS").cy;
        RECT segment = bounds;
        const auto drawRate = [&](const std::wstring& label) {
            segment.right = (std::min)(bounds.right, segment.left + extent(label).cx);
            if (segment.left < segment.right) DrawTextW(dc, label.c_str(), -1, &segment, flags);
            segment.left = (std::min)(bounds.right, segment.right + gap);
        };
        drawRate(original.str());
        const auto blue = SetTextColor(dc, orange);
        drawRate(interpolated.str());
        SetTextColor(dc, blue);

        const auto& extraMs = anrPerformance_->extraMs;
        const auto badge = extraMs ? L"ANR +" + std::to_wstring(std::lround(*extraMs)) + L" ms" : std::wstring{};
        const LONG badgeWidth = badge.empty() ? 0 : extent(badge).cx + 2 * inset;
        const bool showBadge = badgeWidth > 0 && bounds.right - segment.left >= badgeWidth + gap;
        const size_t lineEnd = status.find(L'\n');
        const auto firstLine = status.substr(0, lineEnd);
        const LONG telemetryRight = showBadge ? bounds.right - badgeWidth - gap : bounds.right;
        segment.right = (std::min)(telemetryRight, segment.left + extent(firstLine).cx);
        if (segment.left < segment.right) DrawTextW(dc, firstLine.c_str(), -1, &segment, flags);
        if (showBadge) {
            const LONG left = segment.right + gap;
            RECT label{left, bounds.top, left + badgeWidth, bounds.top + lineHeight};
            PanelFill(dc, label, RGB(75, 48, 26));
            label.left += inset;
            SetTextColor(dc, orange);
            DrawTextW(dc, badge.c_str(), -1, &label, flags);
            SetTextColor(dc, blue);
        }
        if (lineEnd != std::wstring::npos) {
            bounds.top += lineHeight;
            DrawTextW(dc, status.c_str() + lineEnd + 1, -1, &bounds,
                DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
        }
    }

    void LogAnrStats(double seconds) {
        if (!anrStream_ && anrSampleDirectory_.empty()) return;
        if (anrStream_ && anrStatsStarted_ != Clock::time_point{}) {
            seconds = std::chrono::duration<double>(Clock::now() - anrStatsStarted_).count();
            if (seconds < .25) return;
        }
        const double averageExtra = AverageAnrMs(anrExtraMs_, anrExtraSamples_);
        const double fallbackFps = anrPresented_[0] / seconds;
        const double realFps = anrPresented_[1] / seconds;
        const double generatedFps = anrPresented_[2] / seconds;
        const double outputFps = fallbackFps + realFps + generatedFps;
        if (CanRenderAnr() && anrCaptured_ > 0) {
            auto extraMs = anrPerformance_ ? anrPerformance_->extraMs : std::nullopt;
            if (anrExtraSamples_ > 0 || (anrPresented_[0] > 0 && !anrPerformance_)) extraMs = averageExtra;
            anrPerformance_ = AnrPerformanceSample{anrCaptured_ / seconds, outputFps, extraMs};
        }
        std::ostringstream stats;
        stats << std::fixed << std::setprecision(2)
            << "[anr-perf] dx12=" << useDx12_
            << " enabled=" << (anrStream_ != nullptr)
            << " multiplier=" << EffectiveAnrMultiplier()
            << " sample_seconds=" << seconds
            << " captured_fps=" << anrCaptured_ / seconds
            << " fallback_fps=" << fallbackFps
            << " real_fps=" << realFps
            << " generated_fps=" << generatedFps
            << " output_fps=" << outputFps
            << " worker_ms=" << AverageAnrMs(anrProcessingMs_, anrPresented_[1])
            << " fallback_ms=" << AverageAnrMs(anrPresentationMs_[0], anrPresented_[0])
            << " real_ms=" << AverageAnrMs(anrPresentationMs_[1], anrPresented_[1])
            << " generated_ms=" << AverageAnrMs(anrPresentationMs_[2], anrPresented_[2])
            << " budget_ms=" << anrLatencyLimitMs_
            << " extra_ms=" << averageExtra
            << " max_extra_ms=" << anrPeakExtraMs_
            << " budget_drops=" << anrBudgetDrops_;
        Log(stats.str());
        ResetAnrStats();
    }

    void ResetAnrStats() {
        anrStatsStarted_ = Clock::now();
        anrCaptured_ = 0;
        anrPresented_ = {};
        anrPresentationMs_ = {};
        anrProcessingMs_ = 0;
        anrExtraMs_ = anrPeakExtraMs_ = 0.0;
        anrExtraSamples_ = anrBudgetDrops_ = 0;
    }

    static double AverageAnrMs(double total, std::uint64_t count) { return count ? total / count : 0.0; }
