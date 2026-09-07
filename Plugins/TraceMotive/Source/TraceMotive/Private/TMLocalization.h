#pragma once



#include "CoreMinimal.h"

#include "Internationalization/Culture.h"

#include "Internationalization/Internationalization.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Serialization/Csv/CsvParser.h"



namespace TMLoc

{

    inline constexpr const TCHAR* Namespace()

    {

        return TEXT("TraceMotive");

    }



    inline bool IsKoreanEditor()

    {

        const auto Culture = FInternationalization::Get().GetCurrentCulture();

        return Culture->GetName().StartsWith(TEXT("ko")) || Culture->GetTwoLetterISOLanguageName().Equals(TEXT("ko"), ESearchCase::IgnoreCase);

    }

    inline constexpr const TCHAR* LanguageConfigSection = TEXT("TraceMotive.Localization");
    inline constexpr const TCHAR* UseKoreanConfigKey = TEXT("bUseKorean");

    inline bool bLanguagePreferenceLoaded = false;
    inline bool bUseKorean = false;

    inline void LoadLanguagePreference()
    {
        if (bLanguagePreferenceLoaded)
        {
            return;
        }

        bUseKorean = IsKoreanEditor();
        if (GConfig)
        {
            GConfig->GetBool(LanguageConfigSection, UseKoreanConfigKey, bUseKorean, GEditorPerProjectIni);
        }
        bLanguagePreferenceLoaded = true;
    }

    inline bool UseKorean()
    {
        LoadLanguagePreference();
        return bUseKorean;
    }

    inline void SetUseKorean(const bool bInUseKorean)
    {
        LoadLanguagePreference();
        bUseKorean = bInUseKorean;
        if (GConfig)
        {
            GConfig->SetBool(LanguageConfigSection, UseKoreanConfigKey, bUseKorean, GEditorPerProjectIni);
            GConfig->Flush(false, GEditorPerProjectIni);
        }
    }

    inline const TMap<FString, FString>& GetKoreanTranslations()
    {
        static TMap<FString, FString> Translations;
        static bool bLoaded = false;
        if (bLoaded)
        {
            return Translations;
        }

        bLoaded = true;
        const auto LoadCsv = [&](const FString& CsvPath)
        {
            FString CsvContents;
            if (!FFileHelper::LoadFileToString(CsvContents, *CsvPath)) return;
            FCsvParser Parser(MoveTemp(CsvContents));
            for (const TArray<const TCHAR*>& Row : Parser.GetRows())
            {
                if (Row.Num() < 2 || FCString::Strcmp(Row[0], TEXT("Key")) == 0 || !Row[0][0] || !Row[1][0]) continue;
                Translations.Add(Row[0], Row[1]);
            }
        };
        const FString LocalizationDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("TraceMotive/Config/Localization"));
        LoadCsv(FPaths::Combine(LocalizationDir, TEXT("TMLocalization.ko.csv")));
        LoadCsv(FPaths::Combine(LocalizationDir, TEXT("TMLocalization.ko.overrides.csv")));
        return Translations;
    }

    inline FText Text(const TCHAR* English, const TCHAR* Korean)

    {

        if (UseKorean())
        {
            if (English)
            {
                if (const FString* Translation = GetKoreanTranslations().Find(English))
                {
                    return FText::FromString(*Translation);
                }
            }

            if (Korean && *Korean)
            {
                return FText::FromString(Korean);
            }
        }

        return FText::FromString(English ? English : TEXT(""));

    }



    inline FString String(const TCHAR* English, const TCHAR* Korean)

    {

        return Text(English, Korean).ToString();

    }

}



