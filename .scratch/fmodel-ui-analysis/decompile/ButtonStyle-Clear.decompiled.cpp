class UButtonStyle-Clear_C : public UCommonButtonStyle
{
public:
    struct FSlateBrush NormalBase = {
        "DrawAs": ESlateBrushDrawType::NoDrawType
    };
    struct FSlateBrush NormalHovered = {
        "TintColor": {
            "SpecifiedColor": FLinearColor(0.635417, 0.635417, 0.635417, 1)
        },
        "DrawAs": ESlateBrushDrawType::NoDrawType
    };
    struct FSlateBrush NormalPressed = {
        "TintColor": {
            "SpecifiedColor": FLinearColor(0.197917, 0.197917, 0.197917, 1)
        },
        "DrawAs": ESlateBrushDrawType::NoDrawType
    };
    struct FSlateBrush SelectedBase = {
        "DrawAs": ESlateBrushDrawType::NoDrawType
    };
    struct FSlateBrush SelectedHovered = {
        "DrawAs": ESlateBrushDrawType::NoDrawType
    };
    struct FSlateBrush SelectedPressed = {
        "DrawAs": ESlateBrushDrawType::NoDrawType
    };
    struct FSlateBrush Disabled = {
        "DrawAs": ESlateBrushDrawType::NoDrawType
    };
};