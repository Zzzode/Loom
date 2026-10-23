fn sequence_autonumber_values_must_be_finite() {
    for (start, step) in [
        (Some(f64::NAN), Some(1.0)),
        (Some(1.0), Some(f64::INFINITY)),
        (Some(f64::NEG_INFINITY), None),
    ] {
        let mut model = basic_sequence_model();
        model.messages.push(SequenceMessage {
            id: "auto".to_string(),
            from: None,
            to: None,
            message_type: LINETYPE_AUTONUMBER,
            message: SequenceMessagePayload::Autonumber(SequenceAutonumber {
                start,
                step,
                visible: true,
            }),
            wrap: false,
            activate: false,
            placement: None,
            central_connection: 0,
        });
        assert_unsupported_sequence_model(model, "autonumber values");
    }
}