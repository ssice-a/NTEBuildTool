class UW_HTButton_Com_02_C : public UHTUI_Button
{
public:
    class UWidgetTree* WidgetTree = "WidgetTree'/Game/UI/Blueprints/W_HTButton_Com_02.W_HTButton_Com_02_C:WidgetTree'";
    bool bClassRequiresNativeTick = true;
    TArray<class UWidgetAnimation*> Animations = {
        "WidgetAnimation'/Game/UI/Blueprints/W_HTButton_Com_02.W_HTButton_Com_02_C:HoveredAnim_INST'",
        "WidgetAnimation'/Game/UI/Blueprints/W_HTButton_Com_02.W_HTButton_Com_02_C:PressedAnim_INST'",
        "WidgetAnimation'/Game/UI/Blueprints/W_HTButton_Com_02.W_HTButton_Com_02_C:ReleasedAnim_INST'",
        "WidgetAnimation'/Game/UI/Blueprints/W_HTButton_Com_02.W_HTButton_Com_02_C:ReceiveNavigationAnim_INST'",
        "WidgetAnimation'/Game/UI/Blueprints/W_HTButton_Com_02.W_HTButton_Com_02_C:LostNavigationAnim_INST'"
    };
    TArray<FName> NamedSlots = {
        FName("NamedSlot_87")
    };
    TArray<FName> AvailableNamedSlots = {
        FName("NamedSlot_87")
    };
    TArray<FName> InstanceNamedSlots = {
        FName("NamedSlot_87")
    };
    class UFunction* UberGraphFunction = "Function'/Game/UI/Blueprints/W_HTButton_Com_02.W_HTButton_Com_02_C:ExecuteUbergraph_W_HTButton_Com_02'";
    TMap<FName, struct FGuid> CookedPropertyGuids = {
        { FName("IconReceivedBrush"), FGuid(0x8EDE9422, 0x4B223FF4, 0x3C5350AB, 0x4702AD30) }, 
        { FName("ShowWenli"), FGuid(0x4C9A7639, 0x4756BFB7, 0x9855FEBF, 0xB6885CA9) }, 
        { FName("btnscalepadding"), FGuid(0x8C64BE0F, 0x449018A4, 0xF81A5CB5, 0x6786299F) }, 
        { FName("ButtonIconBrush"), FGuid(0x864DBA9C, 0x40CCB78C, 0xB3D87CA2, 0x2896936D) }, 
        { FName("ButtonIconBgBrush"), FGuid(0x6CDC0EA0, 0x4BDADB3D, 0xCBD30A83, 0x0601CA1E) }, 
        { FName("WrapTextAt"), FGuid(0x4583B76F, 0x43FA5542, 0xD6944E8E, 0xA9CF1A17) }, 
        { FName("Justification"), FGuid(0xFB8E2519, 0x481AE013, 0xE9AB9D82, 0x03B16D0F) }, 
        { FName("IconDownBrush"), FGuid(0x85F19169, 0x4729A926, 0x54EF8B86, 0x9B95BB52) }, 
        { FName("IconGlowBrush"), FGuid(0x81CFE3C0, 0x4518F073, 0x0B1C32AF, 0x7F7A1EBB) }, 
        { FName("In Line Height Percentage"), FGuid(0x3DB223C9, 0x4EE54EB3, 0xE7824699, 0xEBD452CB) }, 
        { FName("SecondDownPadding"), FGuid(0x3C2CAF0F, 0x40264D79, 0xBBAD7DAB, 0x808D647A) }, 
        { FName("SecondTopPadding"), FGuid(0x948497E0, 0x45E928CB, 0x9D2B8F9E, 0x86E3D0A2) }, 
        { FName("SecondRightPadding"), FGuid(0x2C79D2C3, 0x4EDB4C7B, 0x7BDFFF87, 0xFBBEBE04) }, 
        { FName("SecondLeftPadding"), FGuid(0x4B30E841, 0x4348F196, 0x159EBEBD, 0x3F99ECD5) }, 
        { FName("SecondIcon"), FGuid(0xF56E5583, 0x4C3AF454, 0x39780486, 0x0A1DB5EF) }, 
        { FName("UseSecondIcon"), FGuid(0x608874A2, 0x4B8AC322, 0x9DC777AE, 0x509CD9CD) }, 
        { FName("TextDisabledStyle"), FGuid(0x9EE83932, 0x48871098, 0xD856D286, 0x45BC1F15) }, 
        { FName("DisabledButtonBorderBrush"), FGuid(0xFE427836, 0x4C76019A, 0x6278DAAA, 0xBD82089C) }, 
        { FName("TextBottomPadding"), FGuid(0x6C028E0B, 0x41A595FE, 0xF9F62ABD, 0x6549228E) }, 
        { FName("TextTopPadding"), FGuid(0x7F6EC24E, 0x44356859, 0x333CCA8F, 0xE27C90BC) }, 
        { FName("TextLeftPadding"), FGuid(0x5EE8CA3E, 0x476949C5, 0xDE1B57BD, 0xE89C646A) }, 
        { FName("NamedSlotVisible"), FGuid(0x48865206, 0x44F8FF2F, 0xD59873A8, 0xD692B4C8) }, 
        { FName("TextStyle"), FGuid(0x6F1615EC, 0x47270C46, 0xCB8693A5, 0x857A4331) }, 
        { FName("ShadowColorAndOpacity"), FGuid(0x4585D828, 0x48CFAC06, 0x5A921997, 0x5962E6EE) }, 
        { FName("ShadowOffset"), FGuid(0x49664FF8, 0x4E987566, 0xCA9D699B, 0x801884B0) }, 
        { FName("InputPadding"), FGuid(0xC5820EF2, 0x400FFCA7, 0xD70617A2, 0x1C8A0B1F) }, 
        { FName("InputVertAlignment"), FGuid(0x44D800CE, 0x4D27EB38, 0xDAFAF7BD, 0xE9412B09) }, 
        { FName("InputHorzAlignment"), FGuid(0x6AC5D5CB, 0x4C808696, 0x8589B182, 0xADFA7B6E) }, 
        { FName("UseIconOverride"), FGuid(0x286A4334, 0x41B6F795, 0x78289C96, 0x8CB40B23) }, 
        { FName("UseScaleChangeSpacers"), FGuid(0x0345D38D, 0x4D0C5A11, 0x20F89B81, 0x5F45C505) }, 
        { FName("UseImageOverlays"), FGuid(0x900A6804, 0x4C3E31EF, 0xB4D94F8A, 0x4A19C76F) }, 
        { FName("IsDisabled"), FGuid(0x9755154C, 0x44CC732C, 0x2CFBA88C, 0x98C375C4) }, 
        { FName("FlipIconXDimension"), FGuid(0x6DBD1D1E, 0x40FAAFF2, 0xF265CF96, 0xEA903E10) }, 
        { FName("TextCase"), FGuid(0x722545D6, 0x4460B767, 0x0AF4E489, 0x9136FE3C) }, 
        { FName("IconBrush"), FGuid(0xF8AA76FE, 0x4C96AE44, 0x4881B2A3, 0xBEA18905) }, 
        { FName("ButtonBorderBrush"), FGuid(0xFCCF49C7, 0x4521545A, 0x86A71991, 0x10DFD903) }, 
        { FName("Font"), FGuid(0xCFDD0E9D, 0x491086D5, 0xA3A8F297, 0x0F11C9AC) }, 
        { FName("PressProgress"), FGuid(0xD6FED08C, 0x45981CBC, 0x9E0AE082, 0xFD602C9E) }, 
        { FName("HitTestPadding_Y"), FGuid(0xCC15C677, 0x4A0B02E0, 0xFF97ED9C, 0xE2390BCD) }, 
        { FName("HitTestPadding_X"), FGuid(0x33645F27, 0x405BDC69, 0x066E1FAB, 0xD203FE3C) }, 
        { FName("AnimBoundSpacer_Left"), FGuid(0x48998C12, 0x79C13238, 0xA75B867F, 0x76F6D897) }, 
        { FName("AnimBoundSpacer_Right"), FGuid(0x196766D9, 0x8BEC332F, 0xBE8A99FB, 0x0E05EA28) }, 
        { FName("ButtonBorder"), FGuid(0xEF0C67DD, 0x9F07357B, 0xB3D973D8, 0x491C4820) }, 
        { FName("ButtonBorderBG"), FGuid(0x2FFC4C3B, 0xF62A351C, 0x91FE99E7, 0xB759D2C7) }, 
        { FName("Glow"), FGuid(0xB3432510, 0x676D34D3, 0xA6C68F00, 0x166CDD25) }, 
        { FName("Icon"), FGuid(0xC8CC2522, 0xD9C23D0A, 0xB360C7F8, 0xDEE153AF) }, 
        { FName("Icon_Down"), FGuid(0x255BE5AE, 0xC9CE3BA7, 0x8D9BEF5C, 0x4057D743) }, 
        { FName("Icon_Glow"), FGuid(0x17F988DC, 0x98B43EEB, 0xBEB7DFA6, 0x7BAD1FB6) }, 
        { FName("IconOvr"), FGuid(0x87BCD615, 0xFF4C3057, 0x8BE7EFB4, 0xE263E028) }, 
        { FName("Image_ButtonIcon"), FGuid(0x11A00087, 0xA2C83F78, 0xB010C5B1, 0xF5685F5A) }, 
        { FName("Image_ButtonIconBg"), FGuid(0xC98E84DC, 0x12E3316E, 0x9F5BA9BE, 0x36143362) }, 
        { FName("Image_receive"), FGuid(0x986E39A4, 0x495ECA23, 0xF2B2E79C, 0xFAF3ECDB) }, 
        { FName("Image_wenli"), FGuid(0x5E3B8821, 0xC33239AD, 0x918EA75F, 0xCCB7ACF1) }, 
        { FName("ImgCheck_Naviable"), FGuid(0xD42B5B29, 0x3EC43CBC, 0x88D559CA, 0xD6487605) }, 
        { FName("ImgCheck_Naviable_Arrow"), FGuid(0xB737F697, 0xD76E3895, 0x8FAD5839, 0x8086DB63) }, 
        { FName("ImgCheck_Naviable_Fx"), FGuid(0x3D4FB3EB, 0x07453993, 0x9DC2903C, 0x05235020) }, 
        { FName("NamedSlot_87"), FGuid(0xBAC562B2, 0xCB3C355C, 0x8F4F5FC9, 0xA9A9D0C8) }, 
        { FName("RealSecondIcon"), FGuid(0xC9D09462, 0x8ACA3F9C, 0xB3200DE2, 0x16C69414) }, 
        { FName("Ring"), FGuid(0xA1E12C35, 0x5A9831BE, 0x8F6A1324, 0xDA0D19F9) }, 
        { FName("ScaleBox_0"), FGuid(0xC727D828, 0x68F7394B, 0xB000C74F, 0xD29C16D7) }, 
        { FName("TextIconSwitch"), FGuid(0x23A5F10B, 0xB3463592, 0xBC62D44F, 0xEC42673C) }, 
        { FName("TextOvr"), FGuid(0x5CC5AC92, 0xF5F8398C, 0xA684A087, 0x7AC6DB96) }, 
        { FName("TextShadow"), FGuid(0x69B12E50, 0xEA0130C3, 0xA487FCBB, 0x1AA900CF) }
    };
    struct FPointerToUberGraphFrame UberGraphFrame = {};
    struct FSlateFontInfo Font = {
        "FontObject": "Font'/Game/Environment/Building/TrainStation/Fonts/MiSans-Extralight.MiSans-Extralight'",
        "FontMaterial": nullptr,
        "OutlineSettings": {
            "OutlineSize": 0,
            "bMiteredCorners": false,
            "bSeparateFillAlpha": false,
            "bApplyOutlineToDropShadows": false,
            "OutlineMaterial": nullptr,
            "OutlineColor": FLinearColor(0, 0, 0, 1)
        },
        "TypefaceFontName": FName("Default"),
        "Size": 20,
        "LetterSpacing": 0,
        "SkewAmount": 0,
        "bForceMonospaced": false,
        "bMaterialIsStencil": false,
        "MonospacedWidth": 1
    };
    struct FSlateBrush ButtonBorderBrush = {
        "TintColor": {
            "SpecifiedColor": FLinearColor(1, 1, 1, 1),
            "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
        },
        "DrawAs": ESlateBrushDrawType::Box,
        "Tiling": ESlateBrushTileType::NoTile,
        "Mirroring": ESlateBrushMirrorType::NoMirror,
        "ImageType": ESlateBrushImageType::NoImage,
        "ImageSize": FVector2D(168, 74),
        "Margin": {
            "Left": 0.5,
            "Top": 0,
            "Right": 0.5,
            "Bottom": 0
        },
        "ResourceObject": "Texture2D'/Game/UI/UI/common/Butten/UI_YH_Common_Button03_Normal.UI_YH_Common_Button03_Normal'",
        "OutlineSettings": {
            "CornerRadii": FVector4(0, 0, 0, 1),
            "Color": {
                "SpecifiedColor": FLinearColor(0, 0, 0, 0),
                "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
            },
            "Width": 0,
            "RoundingType": ESlateBrushRoundingType::HalfHeightRadius,
            "bUseBrushTransparency": false
        },
        "UVRegion": bIsValid=0, Min=(X: 0, Y: 0), Max=(X: 0, Y: 0),
        "bIsDynamicallyLoaded": false,
        "ResourceName": FName("None")
    };
    struct FSlateBrush IconBrush = {
        "TintColor": {
            "SpecifiedColor": FLinearColor(1, 1, 1, 1),
            "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
        },
        "DrawAs": ESlateBrushDrawType::Image,
        "Tiling": ESlateBrushTileType::NoTile,
        "Mirroring": ESlateBrushMirrorType::NoMirror,
        "ImageType": ESlateBrushImageType::NoImage,
        "ImageSize": FVector2D(32, 32),
        "Margin": {
            "Left": 0,
            "Top": 0,
            "Right": 0,
            "Bottom": 0
        },
        "ResourceObject": nullptr,
        "OutlineSettings": {
            "CornerRadii": FVector4(0, 0, 0, 1),
            "Color": {
                "SpecifiedColor": FLinearColor(0, 0, 0, 0),
                "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
            },
            "Width": 0,
            "RoundingType": ESlateBrushRoundingType::HalfHeightRadius,
            "bUseBrushTransparency": false
        },
        "UVRegion": bIsValid=0, Min=(X: 0, Y: 0), Max=(X: 0, Y: 0),
        "bIsDynamicallyLoaded": false,
        "ResourceName": FName("None")
    };
    struct FMargin InputPadding = {
        "Left": -12,
        "Top": 0,
        "Right": 0,
        "Bottom": 0
    };
    struct FVector2D ShadowOffset = FVector2D(0, 0);
    struct FLinearColor ShadowColorAndOpacity = FLinearColor(0, 0, 0, 0);
    class UBlueprintGeneratedClass* TextStyle = "BlueprintGeneratedClass'/Game/UI/Blueprints/UI_Style/TextStyle-ButtonBold.TextStyle-ButtonBold_C'";
    bool NamedSlotVisible = true;
    struct FSlateBrush DisabledButtonBorderBrush = {
        "TintColor": {
            "SpecifiedColor": FLinearColor(1, 1, 1, 1),
            "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
        },
        "DrawAs": ESlateBrushDrawType::Box,
        "Tiling": ESlateBrushTileType::NoTile,
        "Mirroring": ESlateBrushMirrorType::NoMirror,
        "ImageType": ESlateBrushImageType::FullColor,
        "ImageSize": FVector2D(168, 74),
        "Margin": {
            "Left": 0.5,
            "Top": 0,
            "Right": 0.5,
            "Bottom": 0
        },
        "ResourceObject": "Texture2D'/Game/UI/UI/common/Butten/UI_YH_Common_Button_Receive_Disble.UI_YH_Common_Button_Receive_Disble'",
        "OutlineSettings": {
            "CornerRadii": FVector4(0, 0, 0, 1),
            "Color": {
                "SpecifiedColor": FLinearColor(0, 0, 0, 0),
                "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
            },
            "Width": 0,
            "RoundingType": ESlateBrushRoundingType::HalfHeightRadius,
            "bUseBrushTransparency": false
        },
        "UVRegion": bIsValid=0, Min=(X: 0, Y: 0), Max=(X: 0, Y: 0),
        "bIsDynamicallyLoaded": false,
        "ResourceName": FName("None")
    };
    float In Line Height Percentage = 0.76;
    struct FSlateBrush IconGlowBrush = {
        "TintColor": {
            "SpecifiedColor": FLinearColor(1, 1, 1, 1),
            "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
        },
        "DrawAs": ESlateBrushDrawType::Box,
        "Tiling": ESlateBrushTileType::NoTile,
        "Mirroring": ESlateBrushMirrorType::NoMirror,
        "ImageType": ESlateBrushImageType::FullColor,
        "ImageSize": FVector2D(168, 74),
        "Margin": {
            "Left": 0.5,
            "Top": 0.5,
            "Right": 0.5,
            "Bottom": 0.5
        },
        "ResourceObject": "Texture2D'/Game/UI/UI/common/Butten/UI_YH_Common_Button03_On.UI_YH_Common_Button03_On'",
        "OutlineSettings": {
            "CornerRadii": FVector4(0, 0, 0, 0),
            "Color": {
                "SpecifiedColor": FLinearColor(0, 0, 0, 0),
                "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
            },
            "Width": 0,
            "RoundingType": ESlateBrushRoundingType::HalfHeightRadius,
            "bUseBrushTransparency": false
        },
        "UVRegion": bIsValid=0, Min=(X: 0, Y: 0), Max=(X: 0, Y: 0),
        "bIsDynamicallyLoaded": false,
        "ResourceName": FName("None")
    };
    struct FSlateBrush IconDownBrush = {
        "TintColor": {
            "SpecifiedColor": FLinearColor(1, 1, 1, 1),
            "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
        },
        "DrawAs": ESlateBrushDrawType::Box,
        "Tiling": ESlateBrushTileType::NoTile,
        "Mirroring": ESlateBrushMirrorType::NoMirror,
        "ImageType": ESlateBrushImageType::FullColor,
        "ImageSize": FVector2D(168, 74),
        "Margin": {
            "Left": 0.5,
            "Top": 0.5,
            "Right": 0.5,
            "Bottom": 0.5
        },
        "ResourceObject": "Texture2D'/Game/UI/UI/common/Butten/UI_YH_Common_Button03_Down.UI_YH_Common_Button03_Down'",
        "OutlineSettings": {
            "CornerRadii": FVector4(0, 0, 0, 0),
            "Color": {
                "SpecifiedColor": FLinearColor(0, 0, 0, 0),
                "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
            },
            "Width": 0,
            "RoundingType": ESlateBrushRoundingType::HalfHeightRadius,
            "bUseBrushTransparency": false
        },
        "UVRegion": bIsValid=0, Min=(X: 0, Y: 0), Max=(X: 0, Y: 0),
        "bIsDynamicallyLoaded": false,
        "ResourceName": FName("None")
    };
    enum ETextJustify Justification = ETextJustify::Center;
    struct FSlateBrush ButtonIconBgBrush = {
        "TintColor": {
            "SpecifiedColor": FLinearColor(1, 1, 1, 1),
            "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
        },
        "DrawAs": ESlateBrushDrawType::Box,
        "Tiling": ESlateBrushTileType::NoTile,
        "Mirroring": ESlateBrushMirrorType::NoMirror,
        "ImageType": ESlateBrushImageType::FullColor,
        "ImageSize": FVector2D(74, 74),
        "Margin": {
            "Left": 0.5,
            "Top": 0,
            "Right": 0.5,
            "Bottom": 0
        },
        "ResourceObject": "Texture2D'/Game/UI/UI/AdventureManual/YH_UI_Btn_IconBG_Normal.YH_UI_Btn_IconBG_Normal'",
        "OutlineSettings": {
            "CornerRadii": FVector4(0, 0, 0, 1),
            "Color": {
                "SpecifiedColor": FLinearColor(0, 0, 0, 0),
                "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
            },
            "Width": 0,
            "RoundingType": ESlateBrushRoundingType::HalfHeightRadius,
            "bUseBrushTransparency": false
        },
        "UVRegion": bIsValid=0, Min=(X: 0, Y: 0), Max=(X: 0, Y: 0),
        "bIsDynamicallyLoaded": false,
        "ResourceName": FName("None")
    };
    struct FSlateBrush ButtonIconBrush = {
        "TintColor": {
            "SpecifiedColor": FLinearColor(0.025187, 0.025187, 0.025187, 1),
            "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
        },
        "DrawAs": ESlateBrushDrawType::Box,
        "Tiling": ESlateBrushTileType::NoTile,
        "Mirroring": ESlateBrushMirrorType::NoMirror,
        "ImageType": ESlateBrushImageType::FullColor,
        "ImageSize": FVector2D(44, 44),
        "Margin": {
            "Left": 0.5,
            "Top": 0,
            "Right": 0.5,
            "Bottom": 0
        },
        "ResourceObject": "Texture2D'/Game/UI/UI/AdventureManual/UI_YH_TSZN_Training_Reward_Icon.UI_YH_TSZN_Training_Reward_Icon'",
        "OutlineSettings": {
            "CornerRadii": FVector4(0, 0, 0, 1),
            "Color": {
                "SpecifiedColor": FLinearColor(0, 0, 0, 0),
                "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
            },
            "Width": 0,
            "RoundingType": ESlateBrushRoundingType::HalfHeightRadius,
            "bUseBrushTransparency": false
        },
        "UVRegion": bIsValid=0, Min=(X: 0, Y: 0), Max=(X: 0, Y: 0),
        "bIsDynamicallyLoaded": false,
        "ResourceName": FName("None")
    };
    struct FMargin btnscalepadding = {
        "Left": 15,
        "Top": -3,
        "Right": 15,
        "Bottom": 0
    };
    bool ShowWenli = true;
    struct FSlateBrush IconReceivedBrush = {
        "TintColor": {
            "SpecifiedColor": FLinearColor(1, 1, 1, 0.4),
            "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
        },
        "DrawAs": ESlateBrushDrawType::RoundedBox,
        "Tiling": ESlateBrushTileType::NoTile,
        "Mirroring": ESlateBrushMirrorType::NoMirror,
        "ImageType": ESlateBrushImageType::FullColor,
        "ImageSize": FVector2D(254, 72),
        "Margin": {
            "Left": 0,
            "Top": 0,
            "Right": 0,
            "Bottom": 0
        },
        "ResourceObject": "Texture2D'/Game/UI/UI/VisionExplore/Transparent.Transparent'",
        "OutlineSettings": {
            "CornerRadii": FVector4(0, 0, 0, 0),
            "Color": {
                "SpecifiedColor": FLinearColor(0.783538, 0.048172, 0.223228, 0),
                "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
            },
            "Width": 13,
            "RoundingType": ESlateBrushRoundingType::HalfHeightRadius,
            "bUseBrushTransparency": false
        },
        "UVRegion": bIsValid=0, Min=(X: 0, Y: 0), Max=(X: 0, Y: 0),
        "bIsDynamicallyLoaded": false,
        "ResourceName": FName("None")
    };
    class UAkAudioEvent* ClickSound = "AkAudioEvent'/Game/WwiseAudio/Events/UI/UI/Play_Sfx_UI_Click_Select.Play_Sfx_UI_Click_Select'";
    float ClickInterval = 0.25;
    bool DoClickEventWhenReleaseAnimationEnd = true;
    class UCurveLinearColor* PressTextColorCurve = "CurveLinearColor'/Game/UI/UI/common/Butten/btn_GeneralResources/Curve_CommonBtnColor_02.Curve_CommonBtnColor_02'";
    int32 MinWidth = 168;
    int32 MinHeight = 74;
    class UBlueprintGeneratedClass* Style = "BlueprintGeneratedClass'/Game/UI/Blueprints/ButtonStyle-Clear.ButtonStyle-Clear_C'";
    bool bApplyAlphaOnDisable = false;
    enum ClickMethod = EButtonClickMethod::PreciseClick;
    FMulticastScriptDelegate OnSelectedChangedBase = ["HandleOnSelectedChanged"];
    FMulticastScriptDelegate OnButtonBaseClicked = ["HandleOnButtonClicked"];
    FMulticastScriptDelegate OnButtonBaseDoubleClicked = ["HandleOnButtonDoubleClicked"];
    FMulticastScriptDelegate OnButtonBaseHovered = ["HandleOnButtonHovered"];
    FMulticastScriptDelegate OnButtonBaseUnhovered = ["HandleOnButtonUnhovered"];
    struct FSlateColor ForegroundColor = {
        "SpecifiedColor": FLinearColor(1, 1, 1, 1),
        "ColorUseRule": ESlateColorStylingMode::UseColor_Specified
    };
    bool bIsFocusable = false;
    bool bHasScriptImplementedTick = false;
    bool bHasScriptImplementedPaint = false;
    class UImage* TextShadow;
    class UOverlay* TextOvr;
    class UWidgetSwitcher* TextIconSwitch;
    class UScaleBox* ScaleBox_0;
    class UImage* Ring;
    class UImage* RealSecondIcon;
    class UNamedSlot* NamedSlot_87;
    class UImage* ImgCheck_Naviable_Fx;
    class UImage* ImgCheck_Naviable_Arrow;
    class UImage* ImgCheck_Naviable;
    class UImage* Image_wenli;
    class UImage* Image_receive;
    class UImage* Image_ButtonIconBg;
    class UImage* Image_ButtonIcon;
    class UOverlay* IconOvr;
    class UImage* Icon_Glow;
    class UImage* Icon_Down;
    class UImage* Icon;
    class UImage* Glow;
    class UImage* ButtonBorderBG;
    class UBorder* ButtonBorder;
    class USpacer* AnimBoundSpacer_Right;
    class USpacer* AnimBoundSpacer_Left;
    double HitTestPadding_X;
    double HitTestPadding_Y;
    double PressProgress;
    ETextTransformPolicy TextCase;
    bool FlipIconXDimension;
    bool IsDisabled;
    bool UseImageOverlays;
    bool UseScaleChangeSpacers;
    bool UseIconOverride;
    EHorizontalAlignment InputHorzAlignment;
    EVerticalAlignment InputVertAlignment;
    double TextLeftPadding;
    double TextTopPadding;
    double TextBottomPadding;
    class UClass TextDisabledStyle;
    bool UseSecondIcon;
    class UTexture2D SecondIcon;
    double SecondLeftPadding;
    double SecondRightPadding;
    double SecondTopPadding;
    double SecondDownPadding;
    double WrapTextAt;

    // (Final, UbergraphFunction, HasDefaults)
    private void ExecuteUbergraph_W_HTButton_Com_02(int EntryPoint)
    {
        goto EntryPoint; // EX_ComputedJump
    
        Label_15:
        IsDisabled = false; // EX_LetBool
    
        UpdateTextStyle(); // EX_LocalVirtualFunction
    
        CallFunc_GetDynamicMaterial_ReturnValue = ButtonBorder->GetDynamicMaterial(); // EX_LetObj
    
        GetButtonBorderBrush(CallFunc_GetButtonBorderBrush_OutBrush); // EX_LocalVirtualFunction
    
        ButtonBorder->SetBrush(CallFunc_GetButtonBorderBrush_OutBrush); // EX_Context
    
        SetMaterialInstanceScalarParam(CallFunc_GetDynamicMaterial_ReturnValue, "bIsDisabled", 0); // EX_LocalVirtualFunction
    
        CallFunc_GetDynamicFontMaterial_ReturnValue = ButtonTextBlock->GetDynamicFontMaterial(); // EX_LetObj
    
        SetMaterialInstanceScalarParam(CallFunc_GetDynamicFontMaterial_ReturnValue, "bIsDisabled", 0); // EX_LocalVirtualFunction
    
        goto Label_1530; // EX_PopExecutionFlow
    
        Label_279:
        ResetMaterials(); // EX_LocalVirtualFunction
    
        return; // EX_PopExecutionFlow
    
        Label_294:
        IsDisabled = true; // EX_LetBool
    
        UpdateTextStyle(); // EX_LocalVirtualFunction
    
        CallFunc_GetDynamicMaterial_ReturnValue_1 = ButtonBorder->GetDynamicMaterial(); // EX_LetObj
    
        GetButtonBorderBrush(CallFunc_GetButtonBorderBrush_OutBrush); // EX_LocalVirtualFunction
    
        ButtonBorder->SetBrush(CallFunc_GetButtonBorderBrush_OutBrush); // EX_Context
    
        SetMaterialInstanceScalarParam(CallFunc_GetDynamicMaterial_ReturnValue_1, "bIsDisabled", 1); // EX_LocalVirtualFunction
    
        CallFunc_GetDynamicFontMaterial_ReturnValue_1 = ButtonTextBlock->GetDynamicFontMaterial(); // EX_LetObj
    
        SetMaterialInstanceScalarParam(CallFunc_GetDynamicFontMaterial_ReturnValue_1, "bIsDisabled", 1); // EX_LocalVirtualFunction
    
        goto Label_279; // EX_Jump
    
        Label_562:
        goto Label_15; // EX_Jump
    
        Label_567:
        UpdateButtonStyles(); // EX_LocalVirtualFunction
    
        ResetMaterials(); // EX_LocalVirtualFunction
    
        EvaluateNamedSlotVisibility(); // EX_LocalVirtualFunction
    
        BothIconAndText(); // EX_LocalVirtualFunction
    
        SetIcon_Glow(); // EX_LocalVirtualFunction
    
        SetIcon_Down(); // EX_LocalVirtualFunction
    
        SetIconReceived(); // EX_LocalVirtualFunction
    
        SetButtonIconImage(); // EX_LocalVirtualFunction
    
        SetButtonIconBgImage(); // EX_LocalVirtualFunction
    
        Temp_byte_Variable = 0x3; // EX_Let
    
        Temp_byte_Variable_1 = 0x1; // EX_Let
    
        Temp_bool_Variable = ShowWenli; // EX_LetBool
    
        Image_wenli->SetVisibility(Temp_bool_Variable ? Temp_byte_Variable : Temp_byte_Variable_1); // EX_Context
    
        ButtonBorderBG->SetVisibility(Temp_bool_Variable ? Temp_byte_Variable : Temp_byte_Variable_1); // EX_Context
    
        return; // EX_PopExecutionFlow
    
        Label_931:
        ButtonTextBlock->SetText(K2Node_Event_InText); // EX_Context
    
        return; // EX_PopExecutionFlow
    
        Label_977:
        if (!UseIconOverride)
            goto Label_1065; // EX_JumpIfNot
    
        UWidgetBlueprintLibrary::SetBrushResourceToTexture(IconBrush, K2Node_Event_normalImg); // EX_CallMath
    
        Icon->SetBrush(IconBrush); // EX_Context
    
        return; // EX_PopExecutionFlow
    
        Label_1065:
        UWidgetBlueprintLibrary::SetBrushResourceToTexture(ButtonBorderBrush, K2Node_Event_normalImg); // EX_CallMath
    
        ButtonBorder->SetBrush(ButtonBorderBrush); // EX_Context
    
        return; // EX_PopExecutionFlow
    
        Label_1135:
        if (!UseIconOverride)
            goto Label_1227; // EX_JumpIfNot
    
        SetBrush(K2Node_Event_Img, IconBrush); // EX_LocalVirtualFunction
    
        Icon->SetBrush(IconBrush); // EX_Context
    
        return; // EX_PopExecutionFlow
    
        Label_1227:
        SetBrush(K2Node_Event_Img, ButtonBorderBrush); // EX_LocalVirtualFunction
    
        ButtonBorder->SetBrush(ButtonBorderBrush); // EX_Context
    
        return; // EX_PopExecutionFlow
    
        Label_1301:
        UpdateTextStyle(); // EX_LocalVirtualFunction
    
        goto Label_567; // EX_Jump
    
        Label_1320:
        goto Label_1301; // EX_Jump
    
        Label_1330:
        ButtonTextBlock->SetJustification(Justification); // EX_Context
    
        goto Label_1330; // EX_PopExecutionFlow
    
        Label_1376:
        K2Node_VariableSet_WrapTextAt_ImplicitCast = Cast<float>(WrapTextAt); // EX_Let
    
        ButtonTextBlock->WrapTextAt = K2Node_VariableSet_WrapTextAt_ImplicitCast; // EX_Let
    
        CallFunc_SlotAsHorizontalBoxSlot_ReturnValue = UWidgetLayoutLibrary::SlotAsHorizontalBoxSlot(ScaleBox_0); // EX_LetObj
    
        CallFunc_SlotAsHorizontalBoxSlot_ReturnValue->SetPadding(btnscalepadding); // EX_Context
    
        return; // EX_PopExecutionFlow
    
        Label_1525:
        goto Label_1376; // EX_Jump
    
        Label_1530:
        return; // EX_Return
    }

    // (BlueprintCosmetic, Event, Public, BlueprintEvent)
    public void Construct()
    {
        ExecuteUbergraph_W_HTButton_Com_02(1525); // EX_LocalFinalFunction
    
        return; // EX_Return
    }

    // (BlueprintCosmetic, Event, Public, BlueprintEvent)
    public void PreConstruct(bool IsDesignTime)
    {
        UberGraphFrame->K2Node_Event_IsDesignTime = IsDesignTime; // EX_LetValueOnPersistentFrame
    
        ExecuteUbergraph_W_HTButton_Com_02(1320); // EX_LocalFinalFunction
    
        return; // EX_Return
    }

    // (Event, Public, HasOutParms, BlueprintEvent)
    public void BP_SetNormalImage(const struct FSlateBrush*& Img)
    {
        UberGraphFrame->K2Node_Event_Img = Img; // EX_LetValueOnPersistentFrame
    
        ExecuteUbergraph_W_HTButton_Com_02(1135); // EX_LocalFinalFunction
    
        return; // EX_Return
    }

    // (Event, Public, BlueprintEvent)
    public void BP_SetNormalImageByTexture2D(class UTexture2D* normalImg)
    {
        UberGraphFrame->K2Node_Event_normalImg = normalImg; // EX_LetValueOnPersistentFrame
    
        ExecuteUbergraph_W_HTButton_Com_02(977); // EX_LocalFinalFunction
    
        return; // EX_Return
    }

    // (Event, Protected, HasOutParms, BlueprintEvent)
    protected void UpdateButtonText(FText*& InText)
    {
        UberGraphFrame->K2Node_Event_InText = InText; // EX_LetValueOnPersistentFrame
    
        ExecuteUbergraph_W_HTButton_Com_02(931); // EX_LocalFinalFunction
    
        return; // EX_Return
    }

    // (Event, Protected, BlueprintEvent)
    protected void BP_OnEnabled()
    {
        ExecuteUbergraph_W_HTButton_Com_02(562); // EX_LocalFinalFunction
    
        return; // EX_Return
    }

    // (Event, Protected, BlueprintEvent)
    protected void BP_OnDisabled()
    {
        ExecuteUbergraph_W_HTButton_Com_02(294); // EX_LocalFinalFunction
    
        return; // EX_Return
    }

    // (Public, BlueprintCallable, BlueprintEvent)
    public void UpdateTextStyle()
    {
        CallFunc_IsValidClass_ReturnValue = TextDisabledStyle; // EX_LetBool
    
        if (!CallFunc_IsValidClass_ReturnValue)
            goto Label_267; // EX_JumpIfNot
    
        if (!IsDisabled)
            goto Label_267; // EX_JumpIfNot
    
        ButtonTextBlock->SetStyle(TextDisabledStyle); // EX_Context
    
        Label_98:
        ButtonTextBlock->SetTextTransformPolicy(TextCase); // EX_Context
    
        ButtonTextBlock->SetShadowColorAndOpacity(ShadowColorAndOpacity); // EX_Context
    
        ButtonTextBlock->SetShadowOffset(ShadowOffset); // EX_Context
    
        ButtonTextBlock->SetLineHeightPercentage(In Line Height Percentage); // EX_Context
    
        return; // EX_Jump
    
        Label_267:
        ButtonTextBlock->SetStyle(TextStyle); // EX_Context
    
        goto Label_98; // EX_Jump
    
        return; // EX_Return
    }

    // (Public, HasOutParms, BlueprintCallable, BlueprintEvent)
    public void GetPressProgress(double& Progress)
    {
        Progress = PressProgress; // EX_Let
    
        return; // EX_Return
    }

    // (Public, BlueprintCallable, BlueprintEvent)
    public void ResetMaterials()
    {
        UUserWidget::StopAnimation(PressedAnim); // EX_FinalFunction
    
        CallFunc_GetDynamicMaterial_ReturnValue = ButtonBorder->GetDynamicMaterial(); // EX_LetObj
    
        CallFunc_IsValid_ReturnValue_1 = CallFunc_GetDynamicMaterial_ReturnValue; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue_1)
            return; // EX_JumpIfNot
    
        CallFunc_GetDynamicFontMaterial_ReturnValue = ButtonTextBlock->GetDynamicFontMaterial(); // EX_LetObj
    
        CallFunc_IsValid_ReturnValue = CallFunc_GetDynamicFontMaterial_ReturnValue; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue)
            return; // EX_JumpIfNot
    
        CallFunc_GetDynamicMaterial_ReturnValue->SetScalarParameterValue("Hover_Animate", 0); // EX_Context
    
        CallFunc_GetDynamicFontMaterial_ReturnValue->SetScalarParameterValue("Hover_Animate", 0); // EX_Context
    
        return; // EX_Return
    }

    // (Public, HasDefaults, BlueprintCallable, BlueprintEvent)
    public void UpdateButtonStyles()
    {
        Temp_byte_Variable_2 = 0x3; // EX_Let
    
        Temp_byte_Variable_3 = 0x1; // EX_Let
    
        Temp_bool_Variable = UseImageOverlays; // EX_LetBool
    
        Ring->SetVisibility(Temp_bool_Variable ? Temp_byte_Variable_2 : Temp_byte_Variable_3); // EX_Context
    
        Glow->SetVisibility(Temp_bool_Variable ? Temp_byte_Variable_2 : Temp_byte_Variable_3); // EX_Context
    
        TextShadow->SetVisibility(Temp_bool_Variable ? Temp_byte_Variable_2 : Temp_byte_Variable_3); // EX_Context
    
        GetButtonBorderBrush(CallFunc_GetButtonBorderBrush_OutBrush); // EX_LocalVirtualFunction
    
        ButtonBorder->SetBrush(CallFunc_GetButtonBorderBrush_OutBrush); // EX_Context
    
        Temp_byte_Variable = 0x3; // EX_Let
    
        Temp_byte_Variable_1 = 0x1; // EX_Let
    
        Temp_bool_Variable_2 = UseScaleChangeSpacers; // EX_LetBool
    
        AnimBoundSpacer_Left->SetVisibility(Temp_bool_Variable_2 ? Temp_byte_Variable : Temp_byte_Variable_1); // EX_Context
    
        AnimBoundSpacer_Right->SetVisibility(Temp_bool_Variable_2 ? Temp_byte_Variable : Temp_byte_Variable_1); // EX_Context
    
        CallFunc_IsValid_ReturnValue_1 = ButtonBorder; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue_1)
            goto Label_1072; // EX_JumpIfNot
    
        CallFunc_SlotAsHorizontalBoxSlot_ReturnValue = UWidgetLayoutLibrary::SlotAsHorizontalBoxSlot(ButtonBorder); // EX_LetObj
    
        CallFunc_IsValid_ReturnValue_2 = CallFunc_SlotAsHorizontalBoxSlot_ReturnValue; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue_2)
            goto Label_1072; // EX_JumpIfNot
    
        K2Node_MakeStruct_Left_ImplicitCast_1 = Cast<float>(HitTestPadding_X); // EX_Let
    
        K2Node_MakeStruct_Margin_1.Left = K2Node_MakeStruct_Left_ImplicitCast_1; // EX_Let
    
        K2Node_MakeStruct_Top_ImplicitCast_1 = Cast<float>(HitTestPadding_Y); // EX_Let
    
        K2Node_MakeStruct_Margin_1.Top = K2Node_MakeStruct_Top_ImplicitCast_1; // EX_Let
    
        K2Node_MakeStruct_Right_ImplicitCast = Cast<float>(HitTestPadding_X); // EX_Let
    
        K2Node_MakeStruct_Margin_1.Right = K2Node_MakeStruct_Right_ImplicitCast; // EX_Let
    
        K2Node_MakeStruct_Bottom_ImplicitCast_1 = Cast<float>(HitTestPadding_Y); // EX_Let
    
        K2Node_MakeStruct_Margin_1.Bottom = K2Node_MakeStruct_Bottom_ImplicitCast_1; // EX_Let
    
        CallFunc_SlotAsHorizontalBoxSlot_ReturnValue = UWidgetLayoutLibrary::SlotAsHorizontalBoxSlot(ButtonBorder); // EX_LetObj
    
        CallFunc_SlotAsHorizontalBoxSlot_ReturnValue->SetPadding(K2Node_MakeStruct_Margin_1); // EX_Context
    
        Label_1072:
        CallFunc_IsValid_ReturnValue = InputActionWidget; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue)
            goto Label_1325; // EX_JumpIfNot
    
        CallFunc_SlotAsOverlaySlot_ReturnValue = UWidgetLayoutLibrary::SlotAsOverlaySlot(InputActionWidget); // EX_LetObj
    
        CallFunc_SlotAsOverlaySlot_ReturnValue->SetPadding(InputPadding); // EX_Context
    
        CallFunc_SlotAsOverlaySlot_ReturnValue = UWidgetLayoutLibrary::SlotAsOverlaySlot(InputActionWidget); // EX_LetObj
    
        CallFunc_SlotAsOverlaySlot_ReturnValue->SetHorizontalAlignment(InputHorzAlignment); // EX_Context
    
        CallFunc_SlotAsOverlaySlot_ReturnValue = UWidgetLayoutLibrary::SlotAsOverlaySlot(InputActionWidget); // EX_LetObj
    
        CallFunc_SlotAsOverlaySlot_ReturnValue->SetVerticalAlignment(InputVertAlignment); // EX_Context
    
        Label_1325:
        if (!UseIconOverride)
            goto Label_1845; // EX_JumpIfNot
    
        TextIconSwitch->SetActiveWidget(IconOvr); // EX_Context
    
        Icon->SetBrush(IconBrush); // EX_Context
    
        Temp_real_Variable = 1; // EX_Let
    
        Temp_bool_Variable_1 = FlipIconXDimension; // EX_LetBool
    
        Temp_real_Variable_1 = -1; // EX_Let
    
        CallFunc_MakeVector2D_X_ImplicitCast = Cast<double>(Temp_bool_Variable_1 ? Temp_real_Variable_1 : Temp_real_Variable); // EX_Let
    
        CallFunc_MakeVector2D_ReturnValue = FVector(CallFunc_MakeVector2D_X_ImplicitCast, 1); // EX_Let
    
        K2Node_MakeStruct_WidgetTransform.Translation = FVector2D(0, 0); // EX_Let
    
        K2Node_MakeStruct_WidgetTransform.Scale = CallFunc_MakeVector2D_ReturnValue; // EX_Let
    
        K2Node_MakeStruct_WidgetTransform.Shear = FVector2D(0, 0); // EX_Let
    
        K2Node_MakeStruct_WidgetTransform.Angle = 0; // EX_Let
    
        Icon->SetRenderTransform(K2Node_MakeStruct_WidgetTransform); // EX_Context
    
        return; // EX_Jump
    
        Label_1845:
        TextIconSwitch->SetActiveWidget(TextOvr); // EX_Context
    
        CallFunc_SlotAsWidgetSwitcherSlot_ReturnValue = UWidgetLayoutLibrary::SlotAsWidgetSwitcherSlot(TextOvr); // EX_LetObj
    
        K2Node_MakeStruct_Left_ImplicitCast = Cast<float>(TextLeftPadding); // EX_Let
    
        K2Node_MakeStruct_Margin.Left = K2Node_MakeStruct_Left_ImplicitCast; // EX_Let
    
        K2Node_MakeStruct_Top_ImplicitCast = Cast<float>(TextTopPadding); // EX_Let
    
        K2Node_MakeStruct_Margin.Top = K2Node_MakeStruct_Top_ImplicitCast; // EX_Let
    
        K2Node_MakeStruct_Margin.Right = 0; // EX_Let
    
        K2Node_MakeStruct_Bottom_ImplicitCast = Cast<float>(TextBottomPadding); // EX_Let
    
        K2Node_MakeStruct_Margin.Bottom = K2Node_MakeStruct_Bottom_ImplicitCast; // EX_Let
    
        CallFunc_SlotAsWidgetSwitcherSlot_ReturnValue->SetPadding(K2Node_MakeStruct_Margin); // EX_Context
    
        return; // EX_Return
    }

    // (Event, Public, HasOutParms, BlueprintCallable, BlueprintEvent)
    public class UTexture2D* BP_GetNormalPhoto()
    {
        if (!UseIconOverride)
            goto Label_67; // EX_JumpIfNot
    
        CallFunc_GetBrushResourceAsTexture2D_ReturnValue_1 = UWidgetBlueprintLibrary::GetBrushResourceAsTexture2D(IconBrush); // EX_LetObj
    
        ReturnValue = CallFunc_GetBrushResourceAsTexture2D_ReturnValue_1; // EX_LetObj
    
        return; // EX_Jump
    
        Label_67:
        CallFunc_GetBrushResourceAsTexture2D_ReturnValue = UWidgetBlueprintLibrary::GetBrushResourceAsTexture2D(ButtonBorderBrush); // EX_LetObj
    
        ReturnValue = CallFunc_GetBrushResourceAsTexture2D_ReturnValue; // EX_LetObj
    
        return ReturnValue; // EX_Return
    }

    // (Public, HasOutParms, BlueprintCallable, BlueprintEvent)
    public void SetBrush(struct FSlateBrush InBrush, struct FSlateBrush*& CurBrush)
    {
        CallFunc_GetBrushResourceAsTexture2D_ReturnValue = UWidgetBlueprintLibrary::GetBrushResourceAsTexture2D(InBrush); // EX_LetObj
    
        CallFunc_IsValid_ReturnValue_1 = CallFunc_GetBrushResourceAsTexture2D_ReturnValue; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue_1)
            goto Label_134; // EX_JumpIfNot
    
        CallFunc_GetBrushResourceAsTexture2D_ReturnValue = UWidgetBlueprintLibrary::GetBrushResourceAsTexture2D(InBrush); // EX_LetObj
    
        UWidgetBlueprintLibrary::SetBrushResourceToTexture(CurBrush, CallFunc_GetBrushResourceAsTexture2D_ReturnValue); // EX_CallMath
    
        return; // EX_Jump
    
        Label_134:
        CallFunc_GetBrushResourceAsMaterial_ReturnValue = UWidgetBlueprintLibrary::GetBrushResourceAsMaterial(InBrush); // EX_LetObj
    
        CallFunc_IsValid_ReturnValue = CallFunc_GetBrushResourceAsMaterial_ReturnValue; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue)
            return; // EX_JumpIfNot
    
        CallFunc_GetBrushResourceAsMaterial_ReturnValue = UWidgetBlueprintLibrary::GetBrushResourceAsMaterial(InBrush); // EX_LetObj
    
        UWidgetBlueprintLibrary::SetBrushResourceToMaterial(CurBrush, CallFunc_GetBrushResourceAsMaterial_ReturnValue); // EX_CallMath
    
        return; // EX_Jump
    
        return; // EX_Return
    }

    // (Public, BlueprintCallable, BlueprintEvent)
    public void EvaluateNamedSlotVisibility()
    {
        Temp_byte_Variable = 0x4; // EX_Let
    
        Temp_byte_Variable_1 = 0x1; // EX_Let
    
        Temp_bool_Variable = NamedSlotVisible; // EX_LetBool
    
        NamedSlot_87->SetVisibility(Temp_bool_Variable ? Temp_byte_Variable : Temp_byte_Variable_1); // EX_Context
    
        return; // EX_Return
    }

    // (Public, HasOutParms, BlueprintCallable, BlueprintEvent, BlueprintPure)
    public void GetButtonBorderBrush(struct FSlateBrush& OutBrush)
    {
        if (!IsDisabled)
            goto Label_118; // EX_JumpIfNot
    
        CallFunc_GetBrushResource_ReturnValue = UWidgetBlueprintLibrary::GetBrushResource(DisabledButtonBorderBrush); // EX_LetObj
    
        CallFunc_IsValid_ReturnValue = CallFunc_GetBrushResource_ReturnValue; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue)
            goto Label_150; // EX_JumpIfNot
    
        OutBrush = DisabledButtonBorderBrush; // EX_Let
    
        return; // EX_Jump
    
        Label_118:
        OutBrush = ButtonBorderBrush; // EX_Let
    
        return; // EX_Jump
    
        Label_150:
        OutBrush = ButtonBorderBrush; // EX_Let
    
        return; // EX_Return
    }

    // (Public, BlueprintCallable, BlueprintEvent)
    public void SetMaterialInstanceScalarParam(class UMaterialInstanceDynamic* MaterialInstance, FName ParamName, double Value)
    {
        CallFunc_IsValid_ReturnValue = MaterialInstance; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue)
            return; // EX_JumpIfNot
    
        CallFunc_SetScalarParameterValue_Value_ImplicitCast = Cast<float>(Value); // EX_Let
    
        MaterialInstance->SetScalarParameterValue(ParamName, CallFunc_SetScalarParameterValue_Value_ImplicitCast); // EX_Context
    
        return; // EX_Return
    }

    // (Public, BlueprintCallable, BlueprintEvent)
    public void BP_SetIcon(class UTexture2D InTexture)
    {
        Icon->SetBrushFromSoftTexture(InTexture, false); // EX_Context
    
        return; // EX_Return
    }

    // (Public, BlueprintCallable, BlueprintEvent)
    public void BothIconAndText()
    {
        CallFunc_IsValidSoftObjectReference_ReturnValue = SecondIcon; // EX_LetBool
    
        CallFunc_BooleanAND_ReturnValue = UseSecondIcon && CallFunc_IsValidSoftObjectReference_ReturnValue; // EX_LetBool
    
        if (!CallFunc_BooleanAND_ReturnValue)
            goto Label_615; // EX_JumpIfNot
    
        CallFunc_IsValid_ReturnValue = RealSecondIcon; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue)
            goto Label_572; // EX_JumpIfNot
    
        RealSecondIcon->SetBrushFromSoftTexture(SecondIcon, true); // EX_Context
    
        CallFunc_SlotAsOverlaySlot_ReturnValue = UWidgetLayoutLibrary::SlotAsOverlaySlot(RealSecondIcon); // EX_LetObj
    
        CallFunc_IsValid_ReturnValue_1 = CallFunc_SlotAsOverlaySlot_ReturnValue; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue_1)
            goto Label_572; // EX_JumpIfNot
    
        K2Node_MakeStruct_Left_ImplicitCast = Cast<float>(SecondLeftPadding); // EX_Let
    
        K2Node_MakeStruct_Margin.Left = K2Node_MakeStruct_Left_ImplicitCast; // EX_Let
    
        K2Node_MakeStruct_Top_ImplicitCast = Cast<float>(SecondTopPadding); // EX_Let
    
        K2Node_MakeStruct_Margin.Top = K2Node_MakeStruct_Top_ImplicitCast; // EX_Let
    
        K2Node_MakeStruct_Right_ImplicitCast = Cast<float>(SecondRightPadding); // EX_Let
    
        K2Node_MakeStruct_Margin.Right = K2Node_MakeStruct_Right_ImplicitCast; // EX_Let
    
        K2Node_MakeStruct_Bottom_ImplicitCast = Cast<float>(SecondDownPadding); // EX_Let
    
        K2Node_MakeStruct_Margin.Bottom = K2Node_MakeStruct_Bottom_ImplicitCast; // EX_Let
    
        CallFunc_SlotAsOverlaySlot_ReturnValue = UWidgetLayoutLibrary::SlotAsOverlaySlot(RealSecondIcon); // EX_LetObj
    
        CallFunc_SlotAsOverlaySlot_ReturnValue->SetPadding(K2Node_MakeStruct_Margin); // EX_Context
    
        Label_572:
        RealSecondIcon->SetVisibility(0x4); // EX_Context
    
        return; // EX_Jump
    
        Label_615:
        RealSecondIcon->SetVisibility(0x1); // EX_Context
    
        return; // EX_Return
    }

    // (Public, BlueprintCallable, BlueprintEvent)
    public void SetIcon_Glow()
    {
        CallFunc_IsValid_ReturnValue = Icon_Glow; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue)
            return; // EX_JumpIfNot
    
        Icon_Glow->SetBrush(IconGlowBrush); // EX_Context
    
        return; // EX_Return
    }

    // (Public, BlueprintCallable, BlueprintEvent)
    public void SetIcon_Down()
    {
        CallFunc_IsValid_ReturnValue = Icon_Down; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue)
            return; // EX_JumpIfNot
    
        Icon_Down->SetBrush(IconDownBrush); // EX_Context
    
        return; // EX_Return
    }

    // (Public, BlueprintCallable, BlueprintEvent)
    public void SetButtonIconImage()
    {
        CallFunc_IsValid_ReturnValue = Image_ButtonIcon; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue)
            return; // EX_JumpIfNot
    
        Image_ButtonIcon->SetBrush(ButtonIconBrush); // EX_Context
    
        return; // EX_Return
    }

    // (Public, BlueprintCallable, BlueprintEvent)
    public void SetButtonIconBgImage()
    {
        CallFunc_IsValid_ReturnValue = Image_ButtonIconBg; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue)
            return; // EX_JumpIfNot
    
        Image_ButtonIconBg->SetBrush(ButtonIconBgBrush); // EX_Context
    
        return; // EX_Return
    }

    // (Public, BlueprintCallable, BlueprintEvent)
    public void SetIconReceived()
    {
        CallFunc_IsValid_ReturnValue = Image_receive; // EX_LetBool
    
        if (!CallFunc_IsValid_ReturnValue)
            return; // EX_JumpIfNot
    
        Image_receive->SetBrush(IconReceivedBrush); // EX_Context
    
        return; // EX_Return
    }
};